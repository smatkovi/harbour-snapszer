#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace Snapszer {

struct Card {
    int suit = 0;
    int rank = 10;
    int playedBy = -1;

    int key() const { return suit * 100 + rank; }
    int points() const;
};

struct DrawnCard {
    Card card;
    int player = -1;
    int order = 0;
};

enum class AiDifficulty { Easy = 0, Normal = 1, Hard = 2, Expert = 3 };
enum class AiActionType { None = 0, Play = 1, Claim = 2, ExchangeTrump = 3, CloseTalon = 4 };
enum class RoundEndReason { None = 0, Claim66 = 1, LastTrick = 2, ClosedTalonFailed = 3 };

struct AiAction {
    AiActionType type = AiActionType::None;
    int handIndex = -1;
    bool declareMarriage = false;
};

struct TrickCommitResult {
    int winner = -1;
    std::vector<DrawnCard> drawn;
    bool roundOver = false;
};

class GameCore {
public:
    explicit GameCore(std::uint32_t seed = 1);

    void newMatch(std::uint32_t seed);
    void newRound();

    const std::vector<Card>& hand(int player) const { return m_hands[player]; }
    const std::vector<Card>& wonCards(int player) const { return m_won[player]; }
    const std::vector<Card>& trick() const { return m_trick; }
    const std::vector<Card>& stock() const { return m_stock; }
    bool hasTrumpCard() const { return m_hasTrumpCard; }
    const Card& trumpCard() const { return m_trumpCard; }
    int trumpSuit() const { return m_trumpSuit; }

    int dealer() const { return m_dealer; }
    int turn() const { return m_turn; }
    int leader() const { return m_leader; }
    bool trickPending() const { return m_trickPending; }
    int pendingTrickWinner() const { return m_pendingTrickWinner; }

    bool talonClosed() const { return m_talonClosed; }
    bool strictPlay() const;
    int faceDownStockSize() const { return static_cast<int>(m_stock.size()); }
    int talonSize() const { return static_cast<int>(m_stock.size()) + (m_hasTrumpCard ? 1 : 0); }

    int cardPoints(int player) const { return m_cardPoints[player]; }
    int marriagePoints(int player) const { return m_marriagePoints[player]; }
    int totalPoints(int player) const;
    int gamePoints(int player) const { return m_gamePoints[player]; }
    bool hasWonTrick(int player) const { return m_hasWonTrick[player]; }

    bool roundOver() const { return m_roundOver; }
    bool matchOver() const { return m_matchOver; }
    int roundWinner() const { return m_roundWinner; }
    int roundAward() const { return m_roundAward; }
    RoundEndReason roundEndReason() const { return m_roundEndReason; }
    int matchWinner() const;

    std::vector<int> legalMoves(int player) const;
    bool isLegalMove(int player, int handIndex) const;
    int marriagePointsForCard(int player, int handIndex) const;
    bool playCard(int player, int handIndex, bool declareMarriage = false);
    TrickCommitResult commitTrick();

    bool canExchangeTrump(int player) const;
    bool exchangeTrump(int player);
    bool canCloseTalon(int player) const;
    bool closeTalon(int player);
    bool canClaim66(int player) const;
    bool claim66(int player);

    AiAction chooseAiAction(int player, AiDifficulty difficulty);

    std::string serializeState() const;
    bool restoreState(const std::string& serialized);
    bool validate(std::string* error = nullptr) const;

    // Exchange the roles of player 0 and player 1. A LAN guest mirrors the
    // host's state this way so that the local player is always player 0.
    void swapPlayers();

private:
    static int other(int player) { return 1 - player; }
    static bool validPlayer(int player) { return player == 0 || player == 1; }
    static int rankStrength(int rank);
    static bool cardBeats(const Card& candidate, const Card& lead, int trumpSuit);

    void dealRound();
    Card takeDeckCard(std::vector<Card>& deck);
    int determineTrickWinner() const;
    void finishRound(int winner, int award, RoundEndReason reason);
    int normalAwardForWinner(int winner) const;
    int closedFailureAward() const;
    int closedSuccessAward() const;
    int aiScoreMove(int player, int handIndex, AiDifficulty difficulty) const;
    int chooseRandomIndex(const std::vector<int>& candidates);
    bool shouldAiClose(int player, AiDifficulty difficulty) const;

    std::array<std::vector<Card>, 2> m_hands;
    std::array<std::vector<Card>, 2> m_won;
    std::vector<Card> m_trick;
    std::vector<Card> m_stock;
    Card m_trumpCard;
    bool m_hasTrumpCard = false;
    int m_trumpSuit = -1;

    int m_dealer = 0;
    int m_turn = 1;
    int m_leader = 1;
    bool m_trickPending = false;
    int m_pendingTrickWinner = -1;
    int m_marriageClaimWindowPlayer = -1;

    std::array<int, 2> m_cardPoints{{0, 0}};
    std::array<int, 2> m_marriagePoints{{0, 0}};
    std::array<bool, 2> m_hasWonTrick{{false, false}};
    std::array<std::array<bool, 4>, 2> m_marriageDeclared{{{{false,false,false,false}}, {{false,false,false,false}}}};

    bool m_talonClosed = false;
    int m_closer = -1;
    int m_closerOpponentPointsAtClose = 0;
    bool m_closerOpponentHadTrickAtClose = false;

    std::array<int, 2> m_gamePoints{{0, 0}};
    bool m_roundOver = false;
    bool m_matchOver = false;
    int m_roundWinner = -1;
    int m_roundAward = 0;
    RoundEndReason m_roundEndReason = RoundEndReason::None;
    int m_roundNumber = 0;

    std::mt19937 m_rng;
};

} // namespace Snapszer
