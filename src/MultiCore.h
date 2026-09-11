#pragma once

#include "GameCore.h"

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace Snapszer {

// Three- and four-player games. The two-player game stays in GameCore.
//
//  HungarianThree  hármas snapszer: 24 cards (with nines), 8 each, the hívó
//                  names trump after four cards and plays alone.
//  AustrianThree   Dreierschnapsen: 20 cards, 6 each plus a two-card talon,
//                  contracts are bid, the declarer exchanges with the talon.
//  HungarianFour   négyes snapszer: 24 cards, 6 each, the hívó calls a card
//                  whose holder is his secret partner.
//  AustrianFour    Bauernschnapsen: 20 cards, 5 each, fixed partners across
//                  the table, contracts are bid.
enum class MultiVariant { HungarianThree = 0, AustrianThree = 1, HungarianFour = 2, AustrianFour = 3 };

enum class MultiPhase {
    ChooseTrump = 0,
    CallCard = 1,
    Bidding = 2,
    Talon = 3,
    Announce = 4,
    Doubling = 5,
    Play = 6,
    RoundOver = 7
};

// Ordered by bidding value (Kontraschnapser and Bauernschnapser share 12).
enum class Contract {
    Normal = 0,
    Bettler = 1,
    Schnapser = 2,
    Gang = 3,
    Zehnergang = 4,
    Kontraschnapser = 5,
    Bauernschnapser = 6,
    Kontrabauernschnapser = 7
};

enum class MultiActionType {
    None = 0,
    ChooseTrump = 1,   // value: suit
    CallCard = 2,      // value: card key
    Bid = 3,           // value: Contract
    Pass = 4,
    Discard = 5,       // value: card key
    Snapszer = 6,
    Double = 7,
    Play = 8,          // value: card key, marriage: declare 20/40
    Claim = 9
};

enum class MultiEndReason { None = 0, Claim66 = 1, LastTrick = 2, ContractMade = 3, ContractFailed = 4 };

struct MultiAction {
    MultiActionType type = MultiActionType::None;
    int value = -1;
    bool marriage = false;

    bool operator==(const MultiAction& other) const
    {
        return type == other.type && value == other.value && marriage == other.marriage;
    }
};

class MultiCore {
public:
    static const int MaxSeats = 4;
    static const int MatchTarget = 24;

    explicit MultiCore(MultiVariant variant = MultiVariant::HungarianThree, std::uint32_t seed = 1);

    void newMatch(MultiVariant variant, std::uint32_t seed);
    bool nextRound();

    static int playersFor(MultiVariant variant);
    static int contractValue(Contract contract);
    static bool isTrumpContract(Contract contract);

    MultiVariant variant() const { return m_variant; }
    int players() const { return m_players; }
    bool hungarian() const;
    MultiPhase phase() const { return m_phase; }
    int dealer() const { return m_dealer; }
    int forehand() const { return (m_dealer + 1) % m_players; }
    int actor() const;

    const std::vector<Card>& hand(int seat) const { return m_hands[seat]; }
    const std::vector<Card>& wonCards(int seat) const { return m_won[seat]; }
    const std::vector<Card>& trick() const { return m_trick; }
    int talonSize() const { return static_cast<int>(m_talon.size()); }
    int trumpSuit() const { return isTrumpContract(m_contract) ? m_chosenTrump : -1; }
    int chosenTrump() const { return m_chosenTrump; }
    int calledCard() const { return m_calledCard; }
    bool calledCardRevealed() const { return m_calledRevealed; }
    int calledCardHolder() const { return m_calledHolder; }

    Contract contract() const { return m_contract; }
    int declarer() const { return m_declarer; }
    int bidHolder() const { return m_bidHolder; }
    bool hasPassed(int seat) const { return (m_passedMask >> seat) & 1; }
    int doubling() const { return m_doubling; }
    int doublingStep() const { return m_doublingStep; }
    int sittingOut() const { return m_sittingOut; }
    bool partnersVisibleTo(int viewer, int seat) const;

    bool trickPending() const { return m_trickPending; }
    int pendingTrickWinner() const { return m_pendingWinner; }
    int turn() const { return m_turn; }
    int tricksPlayed() const { return m_tricksPlayed; }
    int tricksTotal() const { return m_tricksTotal; }
    int trickCount(int seat) const { return m_tricksWon[seat]; }
    int cardPoints(int seat) const { return m_cardPoints[seat]; }
    int marriagePoints(int seat) const { return m_marriagePoints[seat]; }
    bool hasWonTrick(int seat) const { return m_tricksWon[seat] > 0; }
    bool inDeclarerParty(int seat) const;
    bool isActive(int seat) const { return seat != m_sittingOut; }
    int activePlayers() const { return m_sittingOut >= 0 ? m_players - 1 : m_players; }

    bool roundOver() const { return m_phase == MultiPhase::RoundOver; }
    int roundWinnerMask() const { return m_roundWinnerMask; }
    int roundAward() const { return m_roundAward; }
    MultiEndReason endReason() const { return m_endReason; }
    int score(int seat) const { return m_scores[seat]; }
    bool matchOver() const { return m_matchOver; }
    int matchWinnerMask() const;

    std::vector<MultiAction> legalActions(int seat) const;
    bool isLegal(int seat, const MultiAction& action) const;
    bool apply(int seat, const MultiAction& action);
    bool canClaim(int seat) const;
    int marriageValue(int seat, int cardKey) const;
    std::vector<int> legalCards(int seat) const;

    // Resolves a complete trick: the winner collects it and contract
    // conditions are checked. Returns the winner or -1.
    int commitTrick();

    MultiAction chooseAiAction(int seat);

    // Seat s becomes (s - offset) mod players, so that seat `offset` of the
    // host is the local player 0 on a LAN guest.
    void rotateSeats(int offset);

    std::string serializeState() const;
    bool restoreState(const std::string& serialized);
    bool validate(std::string* error = nullptr) const;

private:
    int next(int seat) const { return (seat + 1) % m_players; }
    int nextActive(int seat) const;
    int partner(int seat) const { return (seat + 2) % m_players; }
    int firstPacket() const;
    int handSize() const;
    int strength(int rank) const;
    bool beats(const Card& candidate, const Card& winner) const;
    int trickWinnerIndex() const;
    int partyMaskOf(int seat) const;
    int teamMask(int seat) const;
    int declarerSideMask() const;
    int seatTotal(int seat) const;
    bool isSchnapserContract() const;
    bool isAllTricksContract() const;
    bool knowsPartner(int seat, int other) const;
    int claimGroup(int seat) const;
    int groupTotal(int mask) const;
    bool groupHasTrick(int mask) const;
    int allActiveMask() const;
    bool marriagesAllowed() const;
    bool biddingEligible(int seat, Contract contract) const;
    bool outbids(int seat, Contract contract) const;
    int doublingActor() const;
    bool doublingEligible(int seat) const;

    void dealRound();
    void dealSecondPacket();
    void startBidding();
    void finishBidding();
    void startDoubling();
    void startPlay();
    void finishRound(int winnerMask, int award, MultiEndReason reason);
    void failContract();
    void makeContract();
    int normalAward(int winnerMask) const;
    void sortHand(int seat);
    int findCard(int seat, int key) const;

    int aiHandStrength(int seat, int trump) const;
    Contract aiPreferredContract(int seat) const;
    int aiChoosePlay(int seat);

    MultiVariant m_variant = MultiVariant::HungarianThree;
    int m_players = 3;
    MultiPhase m_phase = MultiPhase::ChooseTrump;
    int m_dealer = 0;

    std::array<std::vector<Card>, MaxSeats> m_hands;
    std::array<std::vector<Card>, MaxSeats> m_won;
    std::vector<Card> m_trick;
    std::vector<Card> m_talon;
    std::vector<Card> m_discards;
    std::vector<Card> m_pending;

    int m_chosenTrump = -1;
    int m_calledCard = -1;
    int m_calledHolder = -1;
    bool m_calledRevealed = false;

    Contract m_contract = Contract::Normal;
    int m_declarer = -1;
    int m_bidHolder = -1;
    int m_passedMask = 0;
    int m_doubling = 1;
    int m_doublingStep = 0;
    int m_doublingAskedMask = 0;
    int m_sittingOut = -1;

    int m_turn = -1;
    bool m_trickPending = false;
    int m_pendingWinner = -1;
    int m_marriageWindowSeat = -1;
    int m_tricksPlayed = 0;
    int m_tricksTotal = 0;

    std::array<int, MaxSeats> m_cardPoints{{0, 0, 0, 0}};
    std::array<int, MaxSeats> m_marriagePoints{{0, 0, 0, 0}};
    std::array<int, MaxSeats> m_tricksWon{{0, 0, 0, 0}};
    std::array<std::array<bool, 4>, MaxSeats> m_marriageDeclared{};

    int m_roundWinnerMask = 0;
    int m_roundAward = 0;
    MultiEndReason m_endReason = MultiEndReason::None;
    std::array<int, MaxSeats> m_scores{{0, 0, 0, 0}};
    bool m_matchOver = false;
    int m_roundNumber = 0;

    std::mt19937 m_rng;
};

} // namespace Snapszer
