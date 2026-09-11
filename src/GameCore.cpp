#include "GameCore.h"

#include <algorithm>
#include <iomanip>
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
    std::vector<Card> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        Card card;
        if (!(in >> card.suit >> card.rank >> card.playedBy))
            return false;
        result.push_back(card);
    }
    cards = std::move(result);
    return true;
}

} // namespace

int Card::points() const
{
    switch (rank) {
    case 14: return 11; // Ace
    case 10: return 10;
    case 13: return 4;  // King
    case 12: return 3;  // Upper / Ober
    case 11: return 2;  // Lower / Unter
    default: return 0;
    }
}

GameCore::GameCore(std::uint32_t seed)
    : m_rng(seed)
{
    newMatch(seed);
}

void GameCore::newMatch(std::uint32_t seed)
{
    m_rng.seed(seed);
    m_gamePoints = {{0, 0}};
    m_matchOver = false;
    m_roundNumber = 0;
    // Randomise the first dealer; subsequent rounds alternate exactly.
    m_dealer = static_cast<int>(m_rng() & 1U);
    dealRound();
}

void GameCore::newRound()
{
    if (!m_roundOver || m_matchOver)
        return;
    m_dealer = other(m_dealer);
    ++m_roundNumber;
    dealRound();
}

Card GameCore::takeDeckCard(std::vector<Card>& deck)
{
    Card card = deck.back();
    deck.pop_back();
    card.playedBy = -1;
    return card;
}

void GameCore::dealRound()
{
    for (auto& cards : m_hands)
        cards.clear();
    for (auto& cards : m_won)
        cards.clear();
    m_trick.clear();
    m_stock.clear();

    m_cardPoints = {{0, 0}};
    m_marriagePoints = {{0, 0}};
    m_hasWonTrick = {{false, false}};
    m_marriageDeclared = {{{{false,false,false,false}}, {{false,false,false,false}}}};

    m_talonClosed = false;
    m_closer = -1;
    m_closerOpponentPointsAtClose = 0;
    m_closerOpponentHadTrickAtClose = false;

    m_roundOver = false;
    m_roundWinner = -1;
    m_roundAward = 0;
    m_roundEndReason = RoundEndReason::None;
    m_trickPending = false;
    m_pendingTrickWinner = -1;
    m_marriageClaimWindowPlayer = -1;

    std::vector<Card> deck;
    deck.reserve(20);
    const int ranks[] = {10, 11, 12, 13, 14};
    for (int suit = 0; suit < 4; ++suit)
        for (int rank : ranks)
            deck.push_back(Card{suit, rank, -1});
    std::shuffle(deck.begin(), deck.end(), m_rng);

    const int forehand = other(m_dealer);
    for (int i = 0; i < 3; ++i)
        m_hands[forehand].push_back(takeDeckCard(deck));
    for (int i = 0; i < 3; ++i)
        m_hands[m_dealer].push_back(takeDeckCard(deck));

    m_trumpCard = takeDeckCard(deck);
    m_hasTrumpCard = true;
    m_trumpSuit = m_trumpCard.suit;

    for (int i = 0; i < 2; ++i)
        m_hands[forehand].push_back(takeDeckCard(deck));
    for (int i = 0; i < 2; ++i)
        m_hands[m_dealer].push_back(takeDeckCard(deck));

    // pop_back() is the next talon draw. There are nine face-down cards.
    m_stock = std::move(deck);
    m_leader = forehand;
    m_turn = forehand;
}

int GameCore::rankStrength(int rank)
{
    switch (rank) {
    case 14: return 5;
    case 10: return 4;
    case 13: return 3;
    case 12: return 2;
    case 11: return 1;
    default: return 0;
    }
}

bool GameCore::cardBeats(const Card& candidate, const Card& lead, int trumpSuit)
{
    if (candidate.suit == lead.suit)
        return rankStrength(candidate.rank) > rankStrength(lead.rank);
    return candidate.suit == trumpSuit && lead.suit != trumpSuit;
}

bool GameCore::strictPlay() const
{
    return m_talonClosed || (!m_hasTrumpCard && m_stock.empty());
}

int GameCore::totalPoints(int player) const
{
    if (!validPlayer(player))
        return 0;
    return m_cardPoints[player] + (m_hasWonTrick[player] ? m_marriagePoints[player] : 0);
}

int GameCore::matchWinner() const
{
    if (!m_matchOver)
        return -1;
    return m_gamePoints[0] >= 7 ? 0 : 1;
}

std::vector<int> GameCore::legalMoves(int player) const
{
    std::vector<int> all;
    if (!validPlayer(player) || m_roundOver || m_trickPending || player != m_turn)
        return all;
    const auto& cards = m_hands[player];
    for (int i = 0; i < static_cast<int>(cards.size()); ++i)
        all.push_back(i);

    if (!strictPlay() || m_trick.empty())
        return all;

    const Card& lead = m_trick.front();
    std::vector<int> sameSuit;
    std::vector<int> higherSameSuit;
    std::vector<int> trumps;
    for (int i : all) {
        const Card& card = cards[static_cast<std::size_t>(i)];
        if (card.suit == lead.suit) {
            sameSuit.push_back(i);
            if (rankStrength(card.rank) > rankStrength(lead.rank))
                higherSameSuit.push_back(i);
        }
        if (card.suit == m_trumpSuit)
            trumps.push_back(i);
    }
    if (!higherSameSuit.empty())
        return higherSameSuit;
    if (!sameSuit.empty())
        return sameSuit;
    if (!trumps.empty())
        return trumps;
    return all;
}

bool GameCore::isLegalMove(int player, int handIndex) const
{
    const std::vector<int> moves = legalMoves(player);
    return std::find(moves.begin(), moves.end(), handIndex) != moves.end();
}

int GameCore::marriagePointsForCard(int player, int handIndex) const
{
    if (!validPlayer(player) || player != m_turn || player != m_leader
        || m_roundOver || m_trickPending || !m_trick.empty())
        return 0;
    const auto& cards = m_hands[player];
    if (handIndex < 0 || handIndex >= static_cast<int>(cards.size()))
        return 0;
    const Card& card = cards[static_cast<std::size_t>(handIndex)];
    if (card.rank != 12 && card.rank != 13)
        return 0;
    if (m_marriageDeclared[player][card.suit])
        return 0;
    const int otherRank = card.rank == 12 ? 13 : 12;
    const bool paired = std::any_of(cards.begin(), cards.end(), [&](const Card& value) {
        return value.suit == card.suit && value.rank == otherRank;
    });
    if (!paired)
        return 0;
    return card.suit == m_trumpSuit ? 40 : 20;
}

bool GameCore::playCard(int player, int handIndex, bool declareMarriage)
{
    if (!isLegalMove(player, handIndex))
        return false;
    const int marriage = marriagePointsForCard(player, handIndex);
    if (declareMarriage && marriage == 0)
        return false;

    Card card = m_hands[player][static_cast<std::size_t>(handIndex)];
    m_hands[player].erase(m_hands[player].begin() + handIndex);
    card.playedBy = player;

    if (declareMarriage) {
        m_marriageDeclared[player][card.suit] = true;
        m_marriagePoints[player] += marriage;
        m_marriageClaimWindowPlayer = player;
    } else {
        m_marriageClaimWindowPlayer = -1;
    }

    m_trick.push_back(card);
    if (m_trick.size() == 1) {
        m_turn = other(player);
    } else {
        m_marriageClaimWindowPlayer = -1;
        m_pendingTrickWinner = determineTrickWinner();
        m_trickPending = true;
        m_turn = m_pendingTrickWinner;
    }
    return true;
}

int GameCore::determineTrickWinner() const
{
    if (m_trick.size() != 2)
        return -1;
    const Card& lead = m_trick[0];
    const Card& response = m_trick[1];
    return cardBeats(response, lead, m_trumpSuit) ? response.playedBy : lead.playedBy;
}

TrickCommitResult GameCore::commitTrick()
{
    TrickCommitResult result;
    if (!m_trickPending || m_trick.size() != 2 || m_roundOver)
        return result;

    const int winner = m_pendingTrickWinner;
    result.winner = winner;
    for (Card card : m_trick) {
        m_cardPoints[winner] += card.points();
        card.playedBy = -1;
        m_won[winner].push_back(card);
    }
    m_hasWonTrick[winner] = true;
    m_trick.clear();
    m_trickPending = false;
    m_pendingTrickWinner = -1;
    m_marriageClaimWindowPlayer = -1;
    m_leader = winner;
    m_turn = winner;

    if (!m_talonClosed && (m_hasTrumpCard || !m_stock.empty())) {
        int order = 0;
        auto drawOne = [&](int player) {
            Card card;
            if (!m_stock.empty()) {
                card = m_stock.back();
                m_stock.pop_back();
            } else if (m_hasTrumpCard) {
                card = m_trumpCard;
                m_hasTrumpCard = false;
            } else {
                return;
            }
            card.playedBy = -1;
            m_hands[player].push_back(card);
            result.drawn.push_back(DrawnCard{card, player, order++});
        };

        // With a proper 20-card deal stock size is 9,7,5,3,1. On the final
        // draw the winner gets the last face-down card and the loser the trump.
        drawOne(winner);
        drawOne(other(winner));
    }

    if (m_hands[0].empty() && m_hands[1].empty()) {
        if (m_talonClosed) {
            finishRound(other(m_closer), closedFailureAward(), RoundEndReason::ClosedTalonFailed);
        } else {
            finishRound(winner, 1, RoundEndReason::LastTrick);
        }
    }
    result.roundOver = m_roundOver;
    return result;
}

bool GameCore::canExchangeTrump(int player) const
{
    if (!validPlayer(player) || m_roundOver || m_talonClosed || !m_hasTrumpCard
        || player != m_turn || player != m_leader || !m_trick.empty() || m_trickPending
        || m_stock.size() < 3)
        return false;
    return std::any_of(m_hands[player].begin(), m_hands[player].end(), [&](const Card& card) {
        return card.suit == m_trumpSuit && card.rank == 11;
    });
}

bool GameCore::exchangeTrump(int player)
{
    if (!canExchangeTrump(player))
        return false;
    auto& cards = m_hands[player];
    auto it = std::find_if(cards.begin(), cards.end(), [&](const Card& card) {
        return card.suit == m_trumpSuit && card.rank == 11;
    });
    Card lower = *it;
    *it = m_trumpCard;
    it->playedBy = -1;
    m_trumpCard = lower;
    m_trumpCard.playedBy = -1;
    return true;
}

bool GameCore::canCloseTalon(int player) const
{
    return validPlayer(player) && !m_roundOver && !m_talonClosed && m_hasTrumpCard
        && player == m_turn && player == m_leader && m_trick.empty() && !m_trickPending
        && !m_stock.empty();
}

bool GameCore::closeTalon(int player)
{
    if (!canCloseTalon(player))
        return false;
    m_talonClosed = true;
    m_closer = player;
    const int opponent = other(player);
    m_closerOpponentPointsAtClose = totalPoints(opponent);
    m_closerOpponentHadTrickAtClose = m_hasWonTrick[opponent];
    return true;
}

bool GameCore::canClaim66(int player) const
{
    if (!validPlayer(player) || m_roundOver || m_trickPending || !m_hasWonTrick[player]
        || totalPoints(player) < 66 || player != m_leader)
        return false;
    if (m_trick.empty())
        return player == m_turn;
    // A leader may claim immediately after announcing a marriage by leading one
    // card of the pair, before the opponent answers.
    return m_trick.size() == 1 && m_trick.front().playedBy == player
        && m_marriageClaimWindowPlayer == player;
}

int GameCore::normalAwardForWinner(int winner) const
{
    const int opponent = other(winner);
    if (!m_hasWonTrick[opponent])
        return 3;
    return totalPoints(opponent) < 33 ? 2 : 1;
}

int GameCore::closedSuccessAward() const
{
    if (!m_closerOpponentHadTrickAtClose)
        return 3;
    return m_closerOpponentPointsAtClose < 33 ? 2 : 1;
}

int GameCore::closedFailureAward() const
{
    return m_closerOpponentHadTrickAtClose ? 2 : 3;
}

void GameCore::finishRound(int winner, int award, RoundEndReason reason)
{
    if (m_roundOver || !validPlayer(winner))
        return;
    m_roundOver = true;
    m_roundWinner = winner;
    m_roundAward = std::max(1, std::min(3, award));
    m_roundEndReason = reason;
    m_gamePoints[winner] += m_roundAward;
    if (m_gamePoints[winner] >= 7)
        m_matchOver = true;
}

bool GameCore::claim66(int player)
{
    if (!canClaim66(player))
        return false;
    int award = normalAwardForWinner(player);
    if (m_talonClosed) {
        if (player == m_closer)
            award = closedSuccessAward();
        else
            award = closedFailureAward();
    }
    finishRound(player, award, m_talonClosed && player != m_closer
                ? RoundEndReason::ClosedTalonFailed : RoundEndReason::Claim66);
    return true;
}

int GameCore::chooseRandomIndex(const std::vector<int>& candidates)
{
    if (candidates.empty())
        return -1;
    std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
    return candidates[dist(m_rng)];
}

int GameCore::aiScoreMove(int player, int handIndex, AiDifficulty difficulty) const
{
    const Card& card = m_hands[player][static_cast<std::size_t>(handIndex)];
    int score = 0;
    const int marriage = marriagePointsForCard(player, handIndex);
    if (marriage)
        score += marriage * (difficulty == AiDifficulty::Normal ? 2 : 3);

    if (m_trick.empty()) {
        // Prefer low-risk leads while preserving trumps and high cards. Hard and
        // Expert value marriage leads strongly via the bonus above.
        score -= card.points() * 3;
        if (card.suit == m_trumpSuit)
            score -= (difficulty >= AiDifficulty::Hard ? 14 : 7);
        if (card.rank == 11)
            score += 5;
        if (strictPlay()) {
            // In strict play, lead winners and trumps more aggressively.
            score += rankStrength(card.rank) * 5;
            if (card.suit == m_trumpSuit)
                score += 12;
        }
    } else {
        const Card& lead = m_trick.front();
        const bool wins = cardBeats(card, lead, m_trumpSuit);
        if (wins) {
            score += 40 + lead.points() * 4;
            score -= card.points();
        } else {
            score -= card.points() * 5;
            if (card.suit == m_trumpSuit)
                score -= 20;
        }
    }

    if (difficulty == AiDifficulty::Expert) {
        // Public-information awareness: retain a trump when the open talon is
        // still large, and shed points more safely as it approaches strict play.
        score += (m_stock.size() <= 3 ? card.points() : -card.points());
        if (card.suit == m_trumpSuit)
            score += m_stock.size() <= 3 ? 8 : -4;
    }
    return score;
}

bool GameCore::shouldAiClose(int player, AiDifficulty difficulty) const
{
    if (!canCloseTalon(player) || difficulty < AiDifficulty::Hard)
        return false;
    const int points = totalPoints(player);
    int strong = 0;
    int trumps = 0;
    for (const Card& card : m_hands[player]) {
        if (card.suit == m_trumpSuit)
            ++trumps;
        if (card.rank == 14 || card.rank == 10)
            ++strong;
    }
    if (difficulty == AiDifficulty::Hard)
        return points >= 53 && (trumps >= 2 || strong >= 3);
    return points >= 48 && (trumps >= 2 || strong >= 3)
        && m_hasWonTrick[player];
}

AiAction GameCore::chooseAiAction(int player, AiDifficulty difficulty)
{
    if (!validPlayer(player) || player != m_turn || m_roundOver || m_trickPending)
        return {};
    if (canClaim66(player))
        return {AiActionType::Claim, -1, false};

    if (canExchangeTrump(player)) {
        if (difficulty != AiDifficulty::Easy || (m_rng() % 100U) < 45U)
            return {AiActionType::ExchangeTrump, -1, false};
    }
    if (shouldAiClose(player, difficulty))
        return {AiActionType::CloseTalon, -1, false};

    const std::vector<int> moves = legalMoves(player);
    if (moves.empty())
        return {};
    int selected = -1;
    if (difficulty == AiDifficulty::Easy) {
        selected = chooseRandomIndex(moves);
    } else {
        int bestScore = -100000;
        std::vector<int> best;
        for (int move : moves) {
            const int value = aiScoreMove(player, move, difficulty);
            if (value > bestScore) {
                bestScore = value;
                best.assign(1, move);
            } else if (value == bestScore) {
                best.push_back(move);
            }
        }
        selected = chooseRandomIndex(best);
        if (difficulty == AiDifficulty::Normal && moves.size() > 1 && (m_rng() % 100U) < 22U)
            selected = chooseRandomIndex(moves);
    }
    const bool declareMarriage = marriagePointsForCard(player, selected) > 0
        && (difficulty != AiDifficulty::Easy || (m_rng() % 100U) < 55U);
    return {AiActionType::Play, selected, declareMarriage};
}

std::string GameCore::serializeState() const
{
    std::ostringstream body;
    body << m_dealer << ' ' << m_turn << ' ' << m_leader << ' '
         << (m_trickPending ? 1 : 0) << ' ' << m_pendingTrickWinner << ' '
         << m_marriageClaimWindowPlayer << '\n';
    writeCards(body, m_hands[0]);
    writeCards(body, m_hands[1]);
    writeCards(body, m_won[0]);
    writeCards(body, m_won[1]);
    writeCards(body, m_trick);
    writeCards(body, m_stock);
    body << (m_hasTrumpCard ? 1 : 0) << ' ' << m_trumpCard.suit << ' '
         << m_trumpCard.rank << ' ' << m_trumpSuit << '\n';
    body << m_cardPoints[0] << ' ' << m_cardPoints[1] << ' '
         << m_marriagePoints[0] << ' ' << m_marriagePoints[1] << ' '
         << (m_hasWonTrick[0] ? 1 : 0) << ' ' << (m_hasWonTrick[1] ? 1 : 0) << '\n';
    for (int player = 0; player < 2; ++player) {
        for (int suit = 0; suit < 4; ++suit)
            body << (m_marriageDeclared[player][suit] ? 1 : 0) << ' ';
    }
    body << '\n';
    body << (m_talonClosed ? 1 : 0) << ' ' << m_closer << ' '
         << m_closerOpponentPointsAtClose << ' ' << (m_closerOpponentHadTrickAtClose ? 1 : 0) << '\n';
    body << m_gamePoints[0] << ' ' << m_gamePoints[1] << ' '
         << (m_roundOver ? 1 : 0) << ' ' << (m_matchOver ? 1 : 0) << ' '
         << m_roundWinner << ' ' << m_roundAward << ' '
         << static_cast<int>(m_roundEndReason) << ' ' << m_roundNumber << '\n';
    body << m_rng << '\n';

    const std::string payload = body.str();
    std::ostringstream out;
    out << "SNAPSZER_STATE_V1\n" << std::hex << checksum(payload) << '\n' << payload;
    return out.str();
}

bool GameCore::restoreState(const std::string& serialized)
{
    std::istringstream envelope(serialized);
    std::string magic, checksumText;
    if (!std::getline(envelope, magic) || magic != "SNAPSZER_STATE_V1"
        || !std::getline(envelope, checksumText))
        return false;
    std::uint64_t expected = 0;
    std::istringstream checkStream(checksumText);
    if (!(checkStream >> std::hex >> expected))
        return false;
    const std::string payload((std::istreambuf_iterator<char>(envelope)),
                              std::istreambuf_iterator<char>());
    if (checksum(payload) != expected)
        return false;

    GameCore restored(1);
    std::istringstream in(payload);
    int trickPending = 0;
    if (!(in >> restored.m_dealer >> restored.m_turn >> restored.m_leader
          >> trickPending >> restored.m_pendingTrickWinner
          >> restored.m_marriageClaimWindowPlayer))
        return false;
    restored.m_trickPending = trickPending != 0;
    if (!readCards(in, restored.m_hands[0], 5)
        || !readCards(in, restored.m_hands[1], 5)
        || !readCards(in, restored.m_won[0], 20)
        || !readCards(in, restored.m_won[1], 20)
        || !readCards(in, restored.m_trick, 2)
        || !readCards(in, restored.m_stock, 9))
        return false;

    int hasTrump = 0;
    if (!(in >> hasTrump >> restored.m_trumpCard.suit >> restored.m_trumpCard.rank
          >> restored.m_trumpSuit))
        return false;
    restored.m_hasTrumpCard = hasTrump != 0;
    restored.m_trumpCard.playedBy = -1;

    int won0 = 0, won1 = 0;
    if (!(in >> restored.m_cardPoints[0] >> restored.m_cardPoints[1]
          >> restored.m_marriagePoints[0] >> restored.m_marriagePoints[1]
          >> won0 >> won1))
        return false;
    restored.m_hasWonTrick = {{won0 != 0, won1 != 0}};
    for (int player = 0; player < 2; ++player) {
        for (int suit = 0; suit < 4; ++suit) {
            int value = 0;
            if (!(in >> value))
                return false;
            restored.m_marriageDeclared[player][suit] = value != 0;
        }
    }

    int closed = 0, oppHadTrick = 0;
    if (!(in >> closed >> restored.m_closer >> restored.m_closerOpponentPointsAtClose >> oppHadTrick))
        return false;
    restored.m_talonClosed = closed != 0;
    restored.m_closerOpponentHadTrickAtClose = oppHadTrick != 0;

    int roundOver = 0, matchOver = 0, reason = 0;
    if (!(in >> restored.m_gamePoints[0] >> restored.m_gamePoints[1]
          >> roundOver >> matchOver >> restored.m_roundWinner >> restored.m_roundAward
          >> reason >> restored.m_roundNumber))
        return false;
    restored.m_roundOver = roundOver != 0;
    restored.m_matchOver = matchOver != 0;
    if (reason < 0 || reason > static_cast<int>(RoundEndReason::ClosedTalonFailed))
        return false;
    restored.m_roundEndReason = static_cast<RoundEndReason>(reason);
    if (!(in >> restored.m_rng))
        return false;

    std::string error;
    if (!restored.validate(&error))
        return false;
    *this = std::move(restored);
    return true;
}

void GameCore::swapPlayers()
{
    auto flip = [](int player) { return validPlayer(player) ? other(player) : player; };
    std::swap(m_hands[0], m_hands[1]);
    std::swap(m_won[0], m_won[1]);
    for (Card& card : m_trick)
        card.playedBy = flip(card.playedBy);

    m_dealer = flip(m_dealer);
    m_turn = flip(m_turn);
    m_leader = flip(m_leader);
    m_pendingTrickWinner = flip(m_pendingTrickWinner);
    m_marriageClaimWindowPlayer = flip(m_marriageClaimWindowPlayer);

    std::swap(m_cardPoints[0], m_cardPoints[1]);
    std::swap(m_marriagePoints[0], m_marriagePoints[1]);
    std::swap(m_hasWonTrick[0], m_hasWonTrick[1]);
    std::swap(m_marriageDeclared[0], m_marriageDeclared[1]);

    m_closer = flip(m_closer);
    std::swap(m_gamePoints[0], m_gamePoints[1]);
    m_roundWinner = flip(m_roundWinner);
}

bool GameCore::validate(std::string* error) const
{
    auto fail = [&](const std::string& message) {
        if (error)
            *error = message;
        return false;
    };
    if (!validPlayer(m_dealer) || !validPlayer(m_turn) || !validPlayer(m_leader))
        return fail("invalid player index");
    if (m_trickPending && !validPlayer(m_pendingTrickWinner))
        return fail("invalid pending trick winner");
    if (!m_trickPending && m_pendingTrickWinner != -1)
        return fail("stale pending trick winner");
    if (m_marriageClaimWindowPlayer < -1 || m_marriageClaimWindowPlayer > 1)
        return fail("invalid marriage claim window");
    if (m_marriageClaimWindowPlayer >= 0
        && (m_trick.size() != 1 || m_trick.front().playedBy != m_marriageClaimWindowPlayer))
        return fail("stale marriage claim window");
    if (m_trick.size() > 2)
        return fail("trick has more than two cards");
    if (m_trickPending != (m_trick.size() == 2))
        return fail("trick pending flag inconsistent");
    if (m_hands[0].size() > 5 || m_hands[1].size() > 5 || m_stock.size() > 9)
        return fail("card group exceeds maximum size");
    if (m_trumpSuit < 0 || m_trumpSuit > 3)
        return fail("invalid trump suit");
    if (m_talonClosed && !validPlayer(m_closer))
        return fail("closed talon without valid closer");
    if (m_gamePoints[0] < 0 || m_gamePoints[1] < 0)
        return fail("negative game points");

    std::set<int> seen;
    std::size_t total = 0;
    auto inspect = [&](const std::vector<Card>& cards, bool allowPlayedBy) {
        for (const Card& card : cards) {
            if (card.suit < 0 || card.suit > 3
                || (card.rank != 10 && card.rank != 11 && card.rank != 12
                    && card.rank != 13 && card.rank != 14))
                return false;
            if (!allowPlayedBy && card.playedBy != -1)
                return false;
            if (allowPlayedBy && !validPlayer(card.playedBy))
                return false;
            if (!seen.insert(card.key()).second)
                return false;
            ++total;
        }
        return true;
    };
    if (!inspect(m_hands[0], false) || !inspect(m_hands[1], false)
        || !inspect(m_won[0], false) || !inspect(m_won[1], false)
        || !inspect(m_stock, false) || !inspect(m_trick, true))
        return fail("invalid or duplicate card");
    if (m_hasTrumpCard) {
        if (m_trumpCard.suit < 0 || m_trumpCard.suit > 3 || m_trumpCard.suit != m_trumpSuit
            || !seen.insert(m_trumpCard.key()).second)
            return fail("invalid or duplicate trump card");
        ++total;
    }
    if (total != 20)
        return fail("game state does not contain exactly 20 cards");

    const int counted0 = [&](){ int v=0; for (const Card& c:m_won[0]) v+=c.points(); return v; }();
    const int counted1 = [&](){ int v=0; for (const Card& c:m_won[1]) v+=c.points(); return v; }();
    if (counted0 != m_cardPoints[0] || counted1 != m_cardPoints[1])
        return fail("card point totals disagree with captured cards");
    return true;
}

} // namespace Snapszer
