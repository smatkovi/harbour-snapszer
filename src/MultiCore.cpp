#include "MultiCore.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <sstream>
#include <utility>

namespace Snapszer {
namespace {

std::uint64_t checksum(const std::string& value)
{
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char byte : value) {
        hash ^= byte;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

void writeCards(std::ostream& out, const std::vector<Card>& cards)
{
    out << cards.size();
    for (const Card& card : cards)
        out << ' ' << card.suit << ' ' << card.rank << ' ' << card.playedBy;
    out << '\n';
}

bool readCards(std::istream& in, std::vector<Card>& cards, std::size_t maxCount)
{
    std::size_t count = 0;
    if (!(in >> count) || count > maxCount)
        return false;
    std::vector<Card> result(count);
    for (Card& card : result) {
        if (!(in >> card.suit >> card.rank >> card.playedBy))
            return false;
    }
    cards = std::move(result);
    return true;
}

int rotateMask(int mask, int offset, int players)
{
    int result = 0;
    for (int seat = 0; seat < players; ++seat) {
        if ((mask >> seat) & 1)
            result |= 1 << ((seat - offset + 2 * players) % players);
    }
    return result;
}

bool isMarriageRank(int rank) { return rank == 12 || rank == 13; }

} // namespace

MultiCore::MultiCore(MultiVariant variant, std::uint32_t seed)
    : m_rng(seed)
{
    newMatch(variant, seed);
}

int MultiCore::playersFor(MultiVariant variant)
{
    return variant == MultiVariant::HungarianFour || variant == MultiVariant::AustrianFour ? 4 : 3;
}

int MultiCore::contractValue(Contract contract)
{
    switch (contract) {
    case Contract::Normal: return 1;
    case Contract::Bettler: return 4;
    case Contract::Schnapser: return 6;
    case Contract::Gang: return 9;
    case Contract::Zehnergang: return 10;
    case Contract::Kontraschnapser: return 12;
    case Contract::Bauernschnapser: return 12;
    case Contract::Kontrabauernschnapser: return 24;
    }
    return 1;
}

bool MultiCore::isTrumpContract(Contract contract)
{
    return contract != Contract::Bettler && contract != Contract::Gang && contract != Contract::Zehnergang;
}

bool MultiCore::hungarian() const
{
    return m_variant == MultiVariant::HungarianThree || m_variant == MultiVariant::HungarianFour;
}

void MultiCore::newMatch(MultiVariant variant, std::uint32_t seed)
{
    m_variant = variant;
    m_players = playersFor(variant);
    m_rng.seed(seed);
    m_scores = {{0, 0, 0, 0}};
    m_matchOver = false;
    m_roundNumber = 0;
    m_dealer = static_cast<int>(m_rng() % static_cast<unsigned>(m_players));
    dealRound();
}

bool MultiCore::nextRound()
{
    if (m_phase != MultiPhase::RoundOver || m_matchOver)
        return false;
    m_dealer = next(m_dealer);
    ++m_roundNumber;
    dealRound();
    return true;
}

int MultiCore::firstPacket() const
{
    return m_variant == MultiVariant::HungarianThree ? 4 : 3;
}

int MultiCore::handSize() const
{
    switch (m_variant) {
    case MultiVariant::HungarianThree: return 8;
    case MultiVariant::AustrianFour: return 5;
    default: return 6;
    }
}

int MultiCore::strength(int rank) const
{
    // The Zehnergang order only applies once that contract has been settled,
    // not while it is merely the highest bid.
    const bool settled = m_phase == MultiPhase::Talon || m_phase == MultiPhase::Doubling
        || m_phase == MultiPhase::Play || m_phase == MultiPhase::RoundOver;
    if (settled && m_contract == Contract::Zehnergang) {
        switch (rank) {
        case 10: return 5;
        case 13: return 4;
        case 12: return 3;
        case 11: return 2;
        case 14: return 1;
        default: return 0;
        }
    }
    switch (rank) {
    case 14: return 5;
    case 10: return 4;
    case 13: return 3;
    case 12: return 2;
    case 11: return 1;
    default: return 0; // nine
    }
}

bool MultiCore::beats(const Card& candidate, const Card& winner) const
{
    const int trump = trumpSuit();
    if (candidate.suit == winner.suit)
        return strength(candidate.rank) > strength(winner.rank);
    return trump >= 0 && candidate.suit == trump;
}

int MultiCore::trickWinnerIndex() const
{
    if (m_trick.empty())
        return -1;
    int best = 0;
    for (int i = 1; i < static_cast<int>(m_trick.size()); ++i) {
        if (beats(m_trick[static_cast<std::size_t>(i)], m_trick[static_cast<std::size_t>(best)]))
            best = i;
    }
    return best;
}

int MultiCore::nextActive(int seat) const
{
    int result = next(seat);
    if (result == m_sittingOut)
        result = next(result);
    return result;
}

void MultiCore::sortHand(int seat)
{
    auto& cards = m_hands[seat];
    std::sort(cards.begin(), cards.end(), [this](const Card& a, const Card& b) {
        if (a.suit != b.suit)
            return a.suit < b.suit;
        return strength(a.rank) > strength(b.rank);
    });
}

int MultiCore::findCard(int seat, int key) const
{
    const auto& cards = m_hands[seat];
    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
        if (cards[static_cast<std::size_t>(i)].key() == key)
            return i;
    }
    return -1;
}

void MultiCore::dealRound()
{
    for (int seat = 0; seat < MaxSeats; ++seat) {
        m_hands[seat].clear();
        m_won[seat].clear();
        m_marriageDeclared[seat] = {{false, false, false, false}};
    }
    m_trick.clear();
    m_talon.clear();
    m_discards.clear();
    m_pending.clear();
    m_cardPoints = {{0, 0, 0, 0}};
    m_marriagePoints = {{0, 0, 0, 0}};
    m_tricksWon = {{0, 0, 0, 0}};

    m_chosenTrump = -1;
    m_calledCard = -1;
    m_calledHolder = -1;
    m_calledRevealed = false;
    m_contract = Contract::Normal;
    m_declarer = -1;
    m_bidHolder = -1;
    m_passedMask = 0;
    m_doubling = 1;
    m_doublingStep = 0;
    m_doublingAskedMask = 0;
    m_sittingOut = -1;
    m_turn = -1;
    m_trickPending = false;
    m_pendingWinner = -1;
    m_marriageWindowSeat = -1;
    m_tricksPlayed = 0;
    m_tricksTotal = handSize();
    m_roundWinnerMask = 0;
    m_roundAward = 0;
    m_endReason = MultiEndReason::None;

    std::vector<Card> deck;
    for (int suit = 0; suit < 4; ++suit) {
        if (hungarian())
            deck.push_back(Card{suit, 9, -1});
        for (int rank = 10; rank <= 14; ++rank)
            deck.push_back(Card{suit, rank, -1});
    }
    std::shuffle(deck.begin(), deck.end(), m_rng);

    auto take = [&deck]() {
        Card card = deck.back();
        deck.pop_back();
        return card;
    };
    for (int i = 0; i < m_players; ++i) {
        const int seat = (forehand() + i) % m_players;
        for (int n = 0; n < firstPacket(); ++n)
            m_hands[seat].push_back(take());
        sortHand(seat);
    }
    if (m_variant == MultiVariant::AustrianThree) {
        m_talon.push_back(take());
        m_talon.push_back(take());
    }
    m_pending = std::move(deck);
    m_phase = m_variant == MultiVariant::HungarianFour ? MultiPhase::CallCard : MultiPhase::ChooseTrump;
}

void MultiCore::dealSecondPacket()
{
    const int count = handSize() - firstPacket();
    for (int i = 0; i < m_players; ++i) {
        const int seat = (forehand() + i) % m_players;
        for (int n = 0; n < count && !m_pending.empty(); ++n) {
            m_hands[seat].push_back(m_pending.back());
            m_pending.pop_back();
        }
        sortHand(seat);
    }
    if (m_calledCard >= 0) {
        for (int seat = 0; seat < m_players; ++seat) {
            if (findCard(seat, m_calledCard) >= 0)
                m_calledHolder = seat;
        }
    }
}

int MultiCore::actor() const
{
    switch (m_phase) {
    case MultiPhase::ChooseTrump:
    case MultiPhase::CallCard:
    case MultiPhase::Announce:
        return forehand();
    case MultiPhase::Bidding:
    case MultiPhase::Play:
        return m_turn;
    case MultiPhase::Talon:
        return m_declarer;
    case MultiPhase::Doubling:
        return doublingActor();
    case MultiPhase::RoundOver:
        return -1;
    }
    return -1;
}

// --- parties -----------------------------------------------------------------

bool MultiCore::inDeclarerParty(int seat) const
{
    if (m_declarer < 0 || seat < 0)
        return false;
    switch (m_variant) {
    case MultiVariant::HungarianThree:
    case MultiVariant::AustrianThree:
        return seat == m_declarer;
    case MultiVariant::HungarianFour:
        return seat == m_declarer || seat == m_calledHolder;
    case MultiVariant::AustrianFour:
        return m_sittingOut >= 0 ? seat == m_declarer : (seat % 2) == (m_declarer % 2);
    }
    return false;
}

int MultiCore::allActiveMask() const
{
    int mask = 0;
    for (int seat = 0; seat < m_players; ++seat) {
        if (isActive(seat))
            mask |= 1 << seat;
    }
    return mask;
}

int MultiCore::partyMaskOf(int seat) const
{
    const bool side = inDeclarerParty(seat);
    int mask = 0;
    for (int other = 0; other < m_players; ++other) {
        if (isActive(other) && inDeclarerParty(other) == side)
            mask |= 1 << other;
    }
    return mask;
}

int MultiCore::teamMask(int seat) const
{
    return (1 << (seat % 2)) | (1 << (seat % 2 + 2));
}

int MultiCore::declarerSideMask() const
{
    if (m_variant == MultiVariant::AustrianFour)
        return teamMask(m_declarer);
    return partyMaskOf(m_declarer);
}

bool MultiCore::groupHasTrick(int mask) const
{
    for (int seat = 0; seat < m_players; ++seat) {
        if (((mask >> seat) & 1) && m_tricksWon[seat] > 0)
            return true;
    }
    return false;
}

int MultiCore::groupTotal(int mask) const
{
    const bool counted = groupHasTrick(mask);
    int total = 0;
    for (int seat = 0; seat < m_players; ++seat) {
        if ((mask >> seat) & 1)
            total += m_cardPoints[seat] + (counted ? m_marriagePoints[seat] : 0);
    }
    return total;
}

int MultiCore::seatTotal(int seat) const
{
    return groupTotal(1 << seat);
}

bool MultiCore::isSchnapserContract() const
{
    return m_contract == Contract::Schnapser || m_contract == Contract::Kontraschnapser;
}

bool MultiCore::isAllTricksContract() const
{
    return m_contract == Contract::Gang || m_contract == Contract::Zehnergang
        || m_contract == Contract::Bauernschnapser || m_contract == Contract::Kontrabauernschnapser;
}

bool MultiCore::partnersVisibleTo(int viewer, int seat) const
{
    if (viewer == seat || m_variant != MultiVariant::HungarianFour || m_calledRevealed)
        return true;
    if (seat == m_declarer)
        return true; // everybody knows where the hívó stands
    // The holder knows he is the partner; a hívó holding his own called card
    // knows he is alone. Everybody else learns it when the card falls.
    return viewer == m_calledHolder;
}

bool MultiCore::knowsPartner(int seat, int other) const
{
    return partnersVisibleTo(seat, other) && inDeclarerParty(seat) == inDeclarerParty(other);
}

int MultiCore::matchWinnerMask() const
{
    if (!m_matchOver)
        return 0;
    const int best = *std::max_element(m_scores.begin(), m_scores.begin() + m_players);
    int mask = 0;
    for (int seat = 0; seat < m_players; ++seat) {
        if (m_scores[seat] == best)
            mask |= 1 << seat;
    }
    return mask;
}

// --- bidding and doubling --------------------------------------------------------

bool MultiCore::biddingEligible(int seat, Contract contract) const
{
    const int fh = forehand();
    const bool isForehand = seat == fh;
    const bool forehandSide = m_variant == MultiVariant::AustrianFour
            ? (seat % 2) == (fh % 2) : isForehand;
    switch (contract) {
    case Contract::Normal:
        return isForehand;
    case Contract::Schnapser:
    case Contract::Bauernschnapser:
        return forehandSide;
    case Contract::Kontraschnapser:
    case Contract::Kontrabauernschnapser:
        return !forehandSide;
    case Contract::Bettler:
    case Contract::Gang:
    case Contract::Zehnergang:
        return true;
    }
    return false;
}

bool MultiCore::outbids(int seat, Contract contract) const
{
    const int value = contractValue(contract);
    const int current = contractValue(m_contract);
    if (value != current)
        return value > current;
    // Equal value: a player with an earlier bidding position may hold it.
    const int fh = forehand();
    const int mine = (seat - fh + m_players) % m_players;
    const int theirs = (m_bidHolder - fh + m_players) % m_players;
    return mine < theirs;
}

void MultiCore::startBidding()
{
    m_phase = MultiPhase::Bidding;
    m_contract = Contract::Normal;
    m_bidHolder = forehand(); // the forehand may not pass the normal game
    m_passedMask = 0;
    m_turn = next(forehand());
}

void MultiCore::finishBidding()
{
    m_declarer = m_bidHolder;
    m_turn = -1;
    if (m_variant == MultiVariant::AustrianFour
        && (m_contract == Contract::Bettler || m_contract == Contract::Gang
            || m_contract == Contract::Zehnergang))
        m_sittingOut = partner(m_declarer);
    if (m_variant == MultiVariant::AustrianThree) {
        for (Card card : m_talon)
            m_hands[m_declarer].push_back(card);
        m_talon.clear();
        sortHand(m_declarer);
        m_phase = MultiPhase::Talon;
        return;
    }
    startDoubling();
}

bool MultiCore::doublingEligible(int seat) const
{
    if (!isActive(seat))
        return false;
    // Kontra and Subkontra come from the defenders, Rekontra from the declarers.
    return (m_doublingStep % 2 == 0) ? !inDeclarerParty(seat) : inDeclarerParty(seat);
}

int MultiCore::doublingActor() const
{
    if (m_phase != MultiPhase::Doubling)
        return -1;
    for (int i = 0; i < m_players; ++i) {
        const int seat = (forehand() + i) % m_players;
        if (doublingEligible(seat) && !((m_doublingAskedMask >> seat) & 1))
            return seat;
    }
    return -1;
}

void MultiCore::startDoubling()
{
    m_phase = MultiPhase::Doubling;
    m_doublingStep = 0;
    m_doublingAskedMask = 0;
    if (doublingActor() < 0)
        startPlay();
}

void MultiCore::startPlay()
{
    m_phase = MultiPhase::Play;
    if (m_contract == Contract::Zehnergang) {
        for (int seat = 0; seat < m_players; ++seat)
            sortHand(seat);
    }
    m_turn = isTrumpContract(m_contract) ? forehand() : m_declarer;
    m_marriageWindowSeat = -1;
}

// --- legality --------------------------------------------------------------------

std::vector<int> MultiCore::legalCards(int seat) const
{
    std::vector<int> result;
    if (m_phase != MultiPhase::Play || m_trickPending || seat != m_turn)
        return result;
    const auto& cards = m_hands[seat];
    std::vector<int> all;
    for (const Card& card : cards)
        all.push_back(card.key());
    if (m_trick.empty())
        return all;

    const Card& lead = m_trick.front();
    const Card& winner = m_trick[static_cast<std::size_t>(trickWinnerIndex())];
    const int trump = trumpSuit();

    std::vector<int> follow, followBeating, trumps, trumpsBeating;
    for (const Card& card : cards) {
        if (card.suit == lead.suit) {
            follow.push_back(card.key());
            if (beats(card, winner))
                followBeating.push_back(card.key());
        } else if (trump >= 0 && card.suit == trump) {
            trumps.push_back(card.key());
            if (beats(card, winner))
                trumpsBeating.push_back(card.key());
        }
    }
    if (!follow.empty()) {
        // Following suit: overtake unless the trick is already trumped.
        return followBeating.empty() ? follow : followBeating;
    }
    if (!trumps.empty())
        return trumpsBeating.empty() ? trumps : trumpsBeating;
    return all;
}

bool MultiCore::marriagesAllowed() const
{
    return m_contract == Contract::Normal || isSchnapserContract();
}

int MultiCore::marriageValue(int seat, int cardKey) const
{
    if (m_phase != MultiPhase::Play || m_trickPending || seat != m_turn || !m_trick.empty()
        || !marriagesAllowed())
        return 0;
    if (isSchnapserContract() && (hungarian() ? !inDeclarerParty(seat) : seat != m_declarer))
        return 0;
    const int index = findCard(seat, cardKey);
    if (index < 0)
        return 0;
    const Card& card = m_hands[seat][static_cast<std::size_t>(index)];
    if (!isMarriageRank(card.rank) || m_marriageDeclared[seat][static_cast<std::size_t>(card.suit)])
        return 0;
    const int pairRank = card.rank == 12 ? 13 : 12;
    if (findCard(seat, card.suit * 100 + pairRank) < 0)
        return 0;
    return card.suit == trumpSuit() ? 40 : 20;
}

int MultiCore::claimGroup(int seat) const
{
    if (m_contract != Contract::Normal)
        return 0;
    if (m_variant == MultiVariant::HungarianFour && !m_calledRevealed)
        return seat == m_declarer ? (1 << seat) : 0;
    return partyMaskOf(seat);
}

bool MultiCore::canClaim(int seat) const
{
    if (m_phase != MultiPhase::Play || m_trickPending || m_contract != Contract::Normal
        || seat < 0 || seat >= m_players || !isActive(seat))
        return false;
    const bool between = m_trick.empty();
    const bool afterMarriage = m_trick.size() == 1 && m_marriageWindowSeat == seat
        && m_trick.front().playedBy == seat;
    if (!between && !afterMarriage)
        return false;
    const int group = claimGroup(seat);
    return group != 0 && groupHasTrick(group) && groupTotal(group) >= 66;
}

std::vector<MultiAction> MultiCore::legalActions(int seat) const
{
    std::vector<MultiAction> actions;
    if (seat < 0 || seat >= m_players)
        return actions;
    switch (m_phase) {
    case MultiPhase::ChooseTrump:
        if (seat == forehand()) {
            for (int suit = 0; suit < 4; ++suit) {
                const bool held = std::any_of(m_hands[seat].begin(), m_hands[seat].end(),
                                              [suit](const Card& c) { return c.suit == suit; });
                if (hungarian() || held)
                    actions.push_back({MultiActionType::ChooseTrump, suit, false});
            }
        }
        break;
    case MultiPhase::CallCard:
        if (seat == forehand()) {
            for (int suit = 0; suit < 4; ++suit)
                for (int rank = 9; rank <= 14; ++rank)
                    actions.push_back({MultiActionType::CallCard, suit * 100 + rank, false});
        }
        break;
    case MultiPhase::Announce:
        if (seat == forehand()) {
            actions.push_back({MultiActionType::Snapszer, -1, false});
            actions.push_back({MultiActionType::Pass, -1, false});
        }
        break;
    case MultiPhase::Bidding:
        if (seat == m_turn) {
            for (int c = 0; c <= static_cast<int>(Contract::Kontrabauernschnapser); ++c) {
                const Contract contract = static_cast<Contract>(c);
                if (biddingEligible(seat, contract) && outbids(seat, contract))
                    actions.push_back({MultiActionType::Bid, c, false});
            }
            actions.push_back({MultiActionType::Pass, -1, false});
        }
        break;
    case MultiPhase::Talon:
        if (seat == m_declarer) {
            for (const Card& card : m_hands[seat])
                actions.push_back({MultiActionType::Discard, card.key(), false});
        }
        break;
    case MultiPhase::Doubling:
        if (seat == doublingActor()) {
            actions.push_back({MultiActionType::Double, -1, false});
            actions.push_back({MultiActionType::Pass, -1, false});
        }
        break;
    case MultiPhase::Play:
        for (int key : legalCards(seat)) {
            actions.push_back({MultiActionType::Play, key, false});
            if (marriageValue(seat, key) > 0)
                actions.push_back({MultiActionType::Play, key, true});
        }
        if (canClaim(seat))
            actions.push_back({MultiActionType::Claim, -1, false});
        break;
    case MultiPhase::RoundOver:
        break;
    }
    return actions;
}

bool MultiCore::isLegal(int seat, const MultiAction& action) const
{
    const auto actions = legalActions(seat);
    return std::find(actions.begin(), actions.end(), action) != actions.end();
}

// --- applying actions --------------------------------------------------------------

bool MultiCore::apply(int seat, const MultiAction& action)
{
    if (!isLegal(seat, action))
        return false;

    switch (action.type) {
    case MultiActionType::ChooseTrump:
        m_chosenTrump = action.value;
        dealSecondPacket();
        if (hungarian()) {
            m_phase = MultiPhase::Announce;
        } else {
            startBidding();
        }
        return true;

    case MultiActionType::CallCard:
        m_calledCard = action.value;
        m_chosenTrump = action.value / 100;
        dealSecondPacket();
        m_phase = MultiPhase::Announce;
        return true;

    case MultiActionType::Snapszer:
        m_declarer = forehand();
        m_contract = Contract::Schnapser;
        startDoubling();
        return true;

    case MultiActionType::Bid:
    case MultiActionType::Pass:
        if (m_phase == MultiPhase::Announce) {
            m_declarer = forehand();
            m_contract = Contract::Normal;
            startDoubling();
            return true;
        }
        if (m_phase == MultiPhase::Doubling) {
            m_doublingAskedMask |= 1 << seat;
            if (doublingActor() < 0)
                startPlay();
            return true;
        }
        // Bidding
        if (action.type == MultiActionType::Bid) {
            m_contract = static_cast<Contract>(action.value);
            m_bidHolder = seat;
        } else {
            m_passedMask |= 1 << seat;
        }
        {
            int active = 0;
            for (int s = 0; s < m_players; ++s) {
                if (!((m_passedMask >> s) & 1))
                    ++active;
            }
            if (active <= 1) {
                finishBidding();
                return true;
            }
            int nextSeat = next(seat);
            while ((m_passedMask >> nextSeat) & 1)
                nextSeat = next(nextSeat);
            m_turn = nextSeat;
        }
        return true;

    case MultiActionType::Discard: {
        const int index = findCard(seat, action.value);
        Card card = m_hands[seat][static_cast<std::size_t>(index)];
        m_hands[seat].erase(m_hands[seat].begin() + index);
        m_discards.push_back(card);
        if (static_cast<int>(m_hands[seat].size()) <= handSize())
            startDoubling();
        return true;
    }

    case MultiActionType::Double:
        m_doubling *= 2;
        ++m_doublingStep;
        m_doublingAskedMask = 0;
        if (m_doublingStep >= 3 || doublingActor() < 0)
            startPlay();
        return true;

    case MultiActionType::Play: {
        const int index = findCard(seat, action.value);
        const int marriage = action.marriage ? marriageValue(seat, action.value) : 0;
        Card card = m_hands[seat][static_cast<std::size_t>(index)];
        m_hands[seat].erase(m_hands[seat].begin() + index);
        card.playedBy = seat;
        if (marriage > 0) {
            m_marriageDeclared[seat][static_cast<std::size_t>(card.suit)] = true;
            m_marriagePoints[seat] += marriage;
            m_marriageWindowSeat = seat;
        } else {
            m_marriageWindowSeat = -1;
        }
        m_trick.push_back(card);
        if (card.key() == m_calledCard)
            m_calledRevealed = true;

        if (static_cast<int>(m_trick.size()) >= activePlayers()) {
            m_pendingWinner = m_trick[static_cast<std::size_t>(trickWinnerIndex())].playedBy;
            m_trickPending = true;
            m_turn = -1;
            m_marriageWindowSeat = -1;
        } else {
            m_turn = nextActive(seat);
        }

        // A Schnapser is made the moment the declarers reach 66, which a
        // marriage can do before the trick is finished.
        if (marriage > 0 && isSchnapserContract()) {
            const int total = hungarian() ? groupTotal(declarerSideMask()) : seatTotal(m_declarer);
            if (total >= 66)
                makeContract();
        }
        return true;
    }

    case MultiActionType::Claim: {
        const int group = claimGroup(seat);
        finishRound(group, normalAward(group), MultiEndReason::Claim66);
        return true;
    }

    case MultiActionType::None:
        break;
    }
    return false;
}

int MultiCore::commitTrick()
{
    if (!m_trickPending || m_phase != MultiPhase::Play)
        return -1;
    const int winner = m_pendingWinner;
    for (Card card : m_trick) {
        m_cardPoints[winner] += card.points();
        card.playedBy = -1;
        m_won[winner].push_back(card);
    }
    ++m_tricksWon[winner];
    ++m_tricksPlayed;
    m_trick.clear();
    m_trickPending = false;
    m_pendingWinner = -1;
    m_marriageWindowSeat = -1;
    m_turn = winner;

    const bool allDone = m_tricksPlayed >= m_tricksTotal;
    switch (m_contract) {
    case Contract::Normal:
        if (allDone) {
            const int mask = m_variant == MultiVariant::AustrianFour ? teamMask(winner) : partyMaskOf(winner);
            finishRound(mask, 1, MultiEndReason::LastTrick);
        }
        break;
    case Contract::Bettler:
        if (winner == m_declarer)
            failContract();
        else if (allDone)
            makeContract();
        break;
    case Contract::Schnapser:
    case Contract::Kontraschnapser:
        if (hungarian()) {
            if (!inDeclarerParty(winner))
                failContract();
            else if (groupTotal(declarerSideMask()) >= 66)
                makeContract();
            else if (allDone)
                failContract();
        } else {
            const int limit = m_variant == MultiVariant::AustrianThree ? 4 : 3;
            if (winner != m_declarer)
                failContract();
            else if (seatTotal(m_declarer) >= 66)
                makeContract();
            else if (m_tricksPlayed >= limit || allDone)
                failContract();
        }
        break;
    case Contract::Gang:
    case Contract::Zehnergang:
    case Contract::Bauernschnapser:
    case Contract::Kontrabauernschnapser:
        if (winner != m_declarer)
            failContract();
        else if (allDone)
            makeContract();
        break;
    }
    return winner;
}

int MultiCore::normalAward(int winnerMask) const
{
    const int losers = allActiveMask() & ~winnerMask;
    if (!groupHasTrick(losers))
        return 3;
    return groupTotal(losers) < 33 ? 2 : 1;
}

void MultiCore::makeContract()
{
    finishRound(declarerSideMask(), contractValue(m_contract), MultiEndReason::ContractMade);
}

void MultiCore::failContract()
{
    const int all = (1 << m_players) - 1;
    finishRound(all & ~declarerSideMask(), contractValue(m_contract), MultiEndReason::ContractFailed);
}

void MultiCore::finishRound(int winnerMask, int award, MultiEndReason reason)
{
    if (m_phase == MultiPhase::RoundOver)
        return;
    m_phase = MultiPhase::RoundOver;
    m_turn = -1;
    m_trickPending = false;
    m_pendingWinner = -1;
    m_marriageWindowSeat = -1;
    m_roundWinnerMask = winnerMask;
    m_roundAward = award * m_doubling;
    m_endReason = reason;
    for (int seat = 0; seat < m_players; ++seat) {
        if ((winnerMask >> seat) & 1)
            m_scores[seat] += m_roundAward;
    }
    m_matchOver = std::any_of(m_scores.begin(), m_scores.begin() + m_players,
                              [](int score) { return score >= MatchTarget; });
}

// --- computer players --------------------------------------------------------------

int MultiCore::aiHandStrength(int seat, int trump) const
{
    int value = 0;
    for (const Card& card : m_hands[seat]) {
        value += card.points();
        if (card.suit == trump)
            value += 6;
    }
    return value;
}

Contract MultiCore::aiPreferredContract(int seat) const
{
    const auto& cards = m_hands[seat];
    int aces = 0, tens = 0, kings = 0, trumps = 0;
    bool trumpAce = false, trumpTen = false;
    for (const Card& card : cards) {
        aces += card.rank == 14;
        tens += card.rank == 10;
        kings += card.rank == 13;
        if (card.suit == m_chosenTrump) {
            ++trumps;
            trumpAce = trumpAce || card.rank == 14;
            trumpTen = trumpTen || card.rank == 10;
        }
    }
    // Every card is the top remaining card of its suit: a safe Gang.
    bool allMasters = true;
    for (const Card& card : cards) {
        for (int rank = hungarian() ? 9 : 10; rank <= 14; ++rank) {
            if (strength(rank) > strength(card.rank) && findCard(seat, card.suit * 100 + rank) < 0)
                allMasters = false;
        }
    }
    // A talon card is still unknown in Dreierschnapsen, so be careful there.
    if (allMasters && m_variant != MultiVariant::AustrianThree)
        return Contract::Gang;
    if (aces == 0 && tens == 0 && kings <= 1)
        return Contract::Bettler;
    if (trumpAce && trumpTen && trumps >= 3 && aces >= 2) {
        if (biddingEligible(seat, Contract::Schnapser))
            return Contract::Schnapser;
        if (biddingEligible(seat, Contract::Kontraschnapser))
            return Contract::Kontraschnapser;
    }
    return Contract::Normal;
}

int MultiCore::aiChoosePlay(int seat)
{
    const std::vector<int> legal = legalCards(seat);
    if (legal.size() == 1)
        return legal.front();
    const int trump = trumpSuit();
    auto cardOf = [](int key) { return Card{key / 100, key % 100, -1}; };
    auto byStrength = [&](bool highest) {
        return *std::max_element(legal.begin(), legal.end(), [&](int a, int b) {
            const Card ca = cardOf(a), cb = cardOf(b);
            const int va = strength(ca.rank) + (ca.suit == trump ? 10 : 0);
            const int vb = strength(cb.rank) + (cb.suit == trump ? 10 : 0);
            return highest ? va < vb : va > vb;
        });
    };
    auto cheapest = [&](const std::vector<int>& keys) {
        return *std::min_element(keys.begin(), keys.end(), [&](int a, int b) {
            const Card ca = cardOf(a), cb = cardOf(b);
            const int va = ca.points() + (ca.suit == trump ? 12 : 0) + strength(ca.rank);
            const int vb = cb.points() + (cb.suit == trump ? 12 : 0) + strength(cb.rank);
            return va < vb;
        });
    };

    const bool declarerSide = inDeclarerParty(seat);
    const bool bettlerDeclarer = m_contract == Contract::Bettler && seat == m_declarer;
    const bool mustTakeAll = (isAllTricksContract() || isSchnapserContract()) && declarerSide;

    if (m_trick.empty()) {
        if (bettlerDeclarer || m_contract == Contract::Bettler)
            return byStrength(false);
        if (mustTakeAll)
            return byStrength(true);
        int bestMarriage = -1, bestValue = 0;
        for (int key : legal) {
            const int value = marriageValue(seat, key);
            if (value > bestValue) {
                bestValue = value;
                bestMarriage = key;
            }
        }
        if (bestMarriage >= 0)
            return bestMarriage;
        for (int key : legal) {
            if (key % 100 == 14 && key / 100 != trump)
                return key;
        }
        return cheapest(legal);
    }

    const Card winner = m_trick[static_cast<std::size_t>(trickWinnerIndex())];
    std::vector<int> winning, losing;
    for (int key : legal)
        (beats(cardOf(key), winner) ? winning : losing).push_back(key);

    if (bettlerDeclarer) {
        if (!losing.empty()) {
            return *std::max_element(losing.begin(), losing.end(), [&](int a, int b) {
                return strength(a % 100) < strength(b % 100);
            });
        }
        return cheapest(winning);
    }
    if (m_contract == Contract::Bettler)
        return losing.empty() ? cheapest(winning) : cheapest(losing);
    if (mustTakeAll || (!declarerSide && (isAllTricksContract() || isSchnapserContract())))
        return winning.empty() ? cheapest(legal) : cheapest(winning);

    int trickPoints = 0;
    for (const Card& card : m_trick)
        trickPoints += card.points();
    const bool lastToPlay = static_cast<int>(m_trick.size()) == activePlayers() - 1;
    if (winner.playedBy != seat && knowsPartner(seat, winner.playedBy)) {
        const bool safe = lastToPlay || winner.rank == 14 || (trump >= 0 && winner.suit == trump);
        if (safe && !losing.empty()) {
            return *std::max_element(losing.begin(), losing.end(), [&](int a, int b) {
                const Card ca = cardOf(a), cb = cardOf(b);
                return ca.points() - (ca.suit == trump ? 20 : 0) < cb.points() - (cb.suit == trump ? 20 : 0);
            });
        }
        return cheapest(losing.empty() ? legal : losing);
    }
    if (!winning.empty() && (trickPoints >= 4 || lastToPlay))
        return cheapest(winning);
    return cheapest(losing.empty() ? legal : losing);
}

MultiAction MultiCore::chooseAiAction(int seat)
{
    const std::vector<MultiAction> actions = legalActions(seat);
    if (actions.empty())
        return {};
    if (canClaim(seat))
        return {MultiActionType::Claim, -1, false};

    switch (m_phase) {
    case MultiPhase::ChooseTrump: {
        MultiAction best = actions.front();
        int bestValue = -1;
        for (const MultiAction& action : actions) {
            const int value = aiHandStrength(seat, action.value) * 4 + static_cast<int>(m_rng() % 4U);
            if (value > bestValue) {
                bestValue = value;
                best = action;
            }
        }
        return best;
    }
    case MultiPhase::CallCard: {
        int bestSuit = 0, bestValue = -1;
        for (int suit = 0; suit < 4; ++suit) {
            const int value = aiHandStrength(seat, suit) * 4 + static_cast<int>(m_rng() % 4U);
            if (value > bestValue) {
                bestValue = value;
                bestSuit = suit;
            }
        }
        for (int rank : {14, 10, 13, 12, 11, 9}) {
            if (findCard(seat, bestSuit * 100 + rank) < 0)
                return {MultiActionType::CallCard, bestSuit * 100 + rank, false};
        }
        return {MultiActionType::CallCard, bestSuit * 100 + 14, false};
    }
    case MultiPhase::Announce: {
        int aces = 0, trumps = 0;
        bool trumpAce = false, trumpTen = false;
        for (const Card& card : m_hands[seat]) {
            aces += card.rank == 14;
            if (card.suit == m_chosenTrump) {
                ++trumps;
                trumpAce = trumpAce || card.rank == 14;
                trumpTen = trumpTen || card.rank == 10;
            }
        }
        const int neededTrumps = m_players == 3 ? 4 : 3;
        if (trumpAce && trumpTen && trumps >= neededTrumps && aces >= 3)
            return {MultiActionType::Snapszer, -1, false};
        return {MultiActionType::Pass, -1, false};
    }
    case MultiPhase::Bidding: {
        const Contract wanted = aiPreferredContract(seat);
        const MultiAction bid{MultiActionType::Bid, static_cast<int>(wanted), false};
        if (wanted != Contract::Normal && isLegal(seat, bid))
            return bid;
        return {MultiActionType::Pass, -1, false};
    }
    case MultiPhase::Talon: {
        const auto& cards = m_hands[seat];
        int chosen = cards.front().key();
        int chosenValue = 1000;
        for (const Card& card : cards) {
            int value;
            if (m_contract == Contract::Bettler) {
                value = -strength(card.rank);
            } else if (!isTrumpContract(m_contract)) {
                value = strength(card.rank);
            } else {
                value = card.points() + (card.suit == m_chosenTrump ? 30 : 0);
                const int pairRank = card.rank == 12 ? 13 : 12;
                if (isMarriageRank(card.rank) && findCard(seat, card.suit * 100 + pairRank) >= 0)
                    value += 15;
            }
            if (value < chosenValue) {
                chosenValue = value;
                chosen = card.key();
            }
        }
        return {MultiActionType::Discard, chosen, false};
    }
    case MultiPhase::Doubling: {
        if (m_doublingStep == 0 && m_contract == Contract::Normal) {
            int aces = 0, trumps = 0;
            for (const Card& card : m_hands[seat]) {
                aces += card.rank == 14;
                trumps += card.suit == m_chosenTrump;
            }
            if (aces >= 2 && trumps >= 3 && (m_rng() % 3U) == 0)
                return {MultiActionType::Double, -1, false};
        }
        return {MultiActionType::Pass, -1, false};
    }
    case MultiPhase::Play: {
        const int key = aiChoosePlay(seat);
        return {MultiActionType::Play, key, marriageValue(seat, key) > 0};
    }
    case MultiPhase::RoundOver:
        break;
    }
    return {};
}

// --- LAN perspective and persistence ------------------------------------------------

void MultiCore::rotateSeats(int offset)
{
    offset = ((offset % m_players) + m_players) % m_players;
    if (offset == 0)
        return;
    const int n = m_players;
    auto seatOf = [n, offset](int seat) { return seat < 0 ? seat : (seat - offset + 2 * n) % n; };
    auto rotateArray = [&](auto& values) {
        auto copy = values;
        for (int seat = 0; seat < n; ++seat)
            values[static_cast<std::size_t>(seatOf(seat))] = copy[static_cast<std::size_t>(seat)];
    };
    rotateArray(m_hands);
    rotateArray(m_won);
    rotateArray(m_cardPoints);
    rotateArray(m_marriagePoints);
    rotateArray(m_tricksWon);
    rotateArray(m_marriageDeclared);
    rotateArray(m_scores);
    for (Card& card : m_trick)
        card.playedBy = seatOf(card.playedBy);
    m_passedMask = rotateMask(m_passedMask, offset, n);
    m_doublingAskedMask = rotateMask(m_doublingAskedMask, offset, n);
    m_roundWinnerMask = rotateMask(m_roundWinnerMask, offset, n);
    m_dealer = seatOf(m_dealer);
    m_declarer = seatOf(m_declarer);
    m_bidHolder = seatOf(m_bidHolder);
    m_sittingOut = seatOf(m_sittingOut);
    m_turn = seatOf(m_turn);
    m_pendingWinner = seatOf(m_pendingWinner);
    m_marriageWindowSeat = seatOf(m_marriageWindowSeat);
    m_calledHolder = seatOf(m_calledHolder);
}

std::string MultiCore::serializeState() const
{
    std::ostringstream body;
    body << static_cast<int>(m_variant) << ' ' << static_cast<int>(m_phase) << ' ' << m_dealer << '\n';
    for (int seat = 0; seat < MaxSeats; ++seat)
        writeCards(body, m_hands[seat]);
    for (int seat = 0; seat < MaxSeats; ++seat)
        writeCards(body, m_won[seat]);
    writeCards(body, m_trick);
    writeCards(body, m_talon);
    writeCards(body, m_discards);
    writeCards(body, m_pending);
    body << m_chosenTrump << ' ' << m_calledCard << ' ' << m_calledHolder << ' '
         << (m_calledRevealed ? 1 : 0) << '\n';
    body << static_cast<int>(m_contract) << ' ' << m_declarer << ' ' << m_bidHolder << ' '
         << m_passedMask << ' ' << m_doubling << ' ' << m_doublingStep << ' '
         << m_doublingAskedMask << ' ' << m_sittingOut << '\n';
    body << m_turn << ' ' << (m_trickPending ? 1 : 0) << ' ' << m_pendingWinner << ' '
         << m_marriageWindowSeat << ' ' << m_tricksPlayed << ' ' << m_tricksTotal << '\n';
    for (int seat = 0; seat < MaxSeats; ++seat) {
        body << m_cardPoints[seat] << ' ' << m_marriagePoints[seat] << ' ' << m_tricksWon[seat];
        for (int suit = 0; suit < 4; ++suit)
            body << ' ' << (m_marriageDeclared[seat][static_cast<std::size_t>(suit)] ? 1 : 0);
        body << ' ' << m_scores[seat] << '\n';
    }
    body << m_roundWinnerMask << ' ' << m_roundAward << ' ' << static_cast<int>(m_endReason) << ' '
         << (m_matchOver ? 1 : 0) << ' ' << m_roundNumber << '\n';
    body << m_rng << '\n';

    const std::string payload = body.str();
    std::ostringstream out;
    out << "SNAPSZER_MULTI_V1\n" << std::hex << checksum(payload) << '\n' << payload;
    return out.str();
}

bool MultiCore::restoreState(const std::string& serialized)
{
    std::istringstream envelope(serialized);
    std::string magic, checksumText;
    if (!std::getline(envelope, magic) || magic != "SNAPSZER_MULTI_V1" || !std::getline(envelope, checksumText))
        return false;
    std::uint64_t expected = 0;
    std::istringstream checkStream(checksumText);
    if (!(checkStream >> std::hex >> expected))
        return false;
    const std::string payload((std::istreambuf_iterator<char>(envelope)), std::istreambuf_iterator<char>());
    if (checksum(payload) != expected)
        return false;

    MultiCore r;
    std::istringstream in(payload);
    int variant = 0, phase = 0;
    if (!(in >> variant >> phase >> r.m_dealer) || variant < 0 || variant > 3 || phase < 0 || phase > 7)
        return false;
    r.m_variant = static_cast<MultiVariant>(variant);
    r.m_players = playersFor(r.m_variant);
    r.m_phase = static_cast<MultiPhase>(phase);
    for (int seat = 0; seat < MaxSeats; ++seat)
        if (!readCards(in, r.m_hands[seat], 8))
            return false;
    for (int seat = 0; seat < MaxSeats; ++seat)
        if (!readCards(in, r.m_won[seat], 24))
            return false;
    if (!readCards(in, r.m_trick, 4) || !readCards(in, r.m_talon, 2) || !readCards(in, r.m_discards, 2)
        || !readCards(in, r.m_pending, 24))
        return false;
    int revealed = 0;
    if (!(in >> r.m_chosenTrump >> r.m_calledCard >> r.m_calledHolder >> revealed))
        return false;
    r.m_calledRevealed = revealed != 0;
    int contract = 0;
    if (!(in >> contract >> r.m_declarer >> r.m_bidHolder >> r.m_passedMask >> r.m_doubling
          >> r.m_doublingStep >> r.m_doublingAskedMask >> r.m_sittingOut)
        || contract < 0 || contract > static_cast<int>(Contract::Kontrabauernschnapser))
        return false;
    r.m_contract = static_cast<Contract>(contract);
    int pending = 0;
    if (!(in >> r.m_turn >> pending >> r.m_pendingWinner >> r.m_marriageWindowSeat >> r.m_tricksPlayed
          >> r.m_tricksTotal))
        return false;
    r.m_trickPending = pending != 0;
    for (int seat = 0; seat < MaxSeats; ++seat) {
        if (!(in >> r.m_cardPoints[seat] >> r.m_marriagePoints[seat] >> r.m_tricksWon[seat]))
            return false;
        for (int suit = 0; suit < 4; ++suit) {
            int value = 0;
            if (!(in >> value))
                return false;
            r.m_marriageDeclared[seat][static_cast<std::size_t>(suit)] = value != 0;
        }
        if (!(in >> r.m_scores[seat]))
            return false;
    }
    int reason = 0, matchOver = 0;
    if (!(in >> r.m_roundWinnerMask >> r.m_roundAward >> reason >> matchOver >> r.m_roundNumber)
        || reason < 0 || reason > 4)
        return false;
    r.m_endReason = static_cast<MultiEndReason>(reason);
    r.m_matchOver = matchOver != 0;
    if (!(in >> r.m_rng))
        return false;
    if (!r.validate())
        return false;
    *this = std::move(r);
    return true;
}

bool MultiCore::validate(std::string* error) const
{
    auto fail = [&](const char* message) {
        if (error)
            *error = message;
        return false;
    };
    auto seatOk = [this](int seat) { return seat >= -1 && seat < m_players; };
    if (m_dealer < 0 || m_dealer >= m_players)
        return fail("invalid dealer");
    if (!seatOk(m_declarer) || !seatOk(m_bidHolder) || !seatOk(m_sittingOut) || !seatOk(m_turn)
        || !seatOk(m_pendingWinner) || !seatOk(m_marriageWindowSeat) || !seatOk(m_calledHolder))
        return fail("invalid seat reference");
    if (m_trickPending != (m_pendingWinner >= 0))
        return fail("inconsistent pending trick");
    if (m_doubling != 1 && m_doubling != 2 && m_doubling != 4 && m_doubling != 8)
        return fail("invalid doubling");

    const std::size_t deckSize = hungarian() ? 24 : 20;
    std::set<int> seen;
    std::size_t total = 0;
    auto inspect = [&](const std::vector<Card>& cards, bool played) {
        for (const Card& card : cards) {
            if (card.suit < 0 || card.suit > 3 || card.rank < (hungarian() ? 9 : 10) || card.rank > 14)
                return false;
            if (played ? (card.playedBy < 0 || card.playedBy >= m_players) : card.playedBy != -1)
                return false;
            if (!seen.insert(card.key()).second)
                return false;
            ++total;
        }
        return true;
    };
    for (int seat = 0; seat < MaxSeats; ++seat) {
        if ((seat >= m_players && (!m_hands[seat].empty() || !m_won[seat].empty()))
            || !inspect(m_hands[seat], false) || !inspect(m_won[seat], false))
            return fail("invalid hand or won pile");
        int points = 0;
        for (const Card& card : m_won[seat])
            points += card.points();
        if (points != m_cardPoints[seat])
            return fail("card points disagree with won cards");
    }
    if (!inspect(m_trick, true) || !inspect(m_talon, false) || !inspect(m_discards, false)
        || !inspect(m_pending, false))
        return fail("invalid card group");
    if (total != deckSize)
        return fail("wrong number of cards");
    return true;
}

} // namespace Snapszer
