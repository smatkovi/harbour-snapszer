// Plays many AI-only matches of every three- and four-player variant and
// checks invariants after each step: state validity, serialisation round
// trips, and that a seat-rotated copy (as a LAN guest keeps it) stays in
// step when it replays the same actions.
#include "MultiCore.h"

#include <cstdio>
#include <cstdlib>
#include <random>

using namespace Snapszer;

namespace {

int failures = 0;

void check(bool condition, const char* what, int line)
{
    if (!condition) {
        std::fprintf(stderr, "FAILED line %d: %s\n", line, what);
        if (++failures > 10)
            std::exit(1);
    }
}
#define CHECK(x) check((x), #x, __LINE__)

std::string rotated(const MultiCore& core, int offset)
{
    MultiCore copy = core;
    copy.rotateSeats(offset);
    return copy.serializeState();
}

} // namespace

// Deterministic checks of single rules, independent of the random matches.
void ruleChecks()
{
    // The forehand is the first to bid and may open with a higher contract.
    for (MultiVariant variant : {MultiVariant::AustrianThree, MultiVariant::AustrianFour}) {
        for (std::uint32_t seed = 1; seed < 40; ++seed) {
            MultiCore core(variant, seed);
            const int fh = core.forehand();
            CHECK(core.apply(fh, core.legalActions(fh).front()));
            CHECK(core.phase() == MultiPhase::Bidding);
            CHECK(core.actor() == fh);
            bool canOpenHigher = false;
            for (const MultiAction& action : core.legalActions(fh))
                canOpenHigher = canOpenHigher || (action.type == MultiActionType::Bid
                                                  && action.value == static_cast<int>(Contract::Schnapser));
            CHECK(canOpenHigher);
            // Opening pass keeps the normal game; everybody else passing ends the bidding.
            CHECK(core.apply(fh, {MultiActionType::Pass, -1, false}));
            CHECK(!core.hasPassed(fh));
            while (core.phase() == MultiPhase::Bidding)
                CHECK(core.apply(core.actor(), {MultiActionType::Pass, -1, false}));
            CHECK(core.declarer() == fh && core.contract() == Contract::Normal);
        }
    }
    // Hungarian four-player: every seat but the hívó is asked for Kontra,
    // whoever holds the called card.
    for (std::uint32_t seed = 1; seed < 60; ++seed) {
        MultiCore core(MultiVariant::HungarianFour, seed);
        const int fh = core.forehand();
        CHECK(core.apply(fh, {MultiActionType::CallCard, 314, false}));
        CHECK(core.apply(fh, {MultiActionType::Pass, -1, false}));
        CHECK(core.phase() == MultiPhase::Doubling);
        int asked = 0;
        while (core.phase() == MultiPhase::Doubling && core.doublingStep() == 0) {
            const int seat = core.actor();
            CHECK(seat != fh);
            const bool mayDouble = core.isLegal(seat, {MultiActionType::Double, -1, false});
            CHECK(mayDouble == !core.inDeclarerParty(seat));
            CHECK(core.apply(seat, {MultiActionType::Pass, -1, false}));
            ++asked;
        }
        CHECK(asked == 3);
    }
}

int main()
{
    ruleChecks();
    std::mt19937 pick(20260911);
    const char* names[] = {"hármas", "Dreierschnapsen", "négyes", "Bauernschnapsen"};
    for (int v = 0; v < 4; ++v) {
        const MultiVariant variant = static_cast<MultiVariant>(v);
        long rounds = 0, actions = 0;
        int contracts[8] = {0};
        int made = 0, failed = 0, claims = 0, lastTricks = 0, doubled = 0, sitOuts = 0;
        for (int match = 0; match < 400; ++match) {
            MultiCore host(variant, 1);
            host.newMatch(variant, pick());
            const int players = host.players();
            const int offset = 1 + static_cast<int>(pick() % static_cast<unsigned>(players - 1));
            MultiCore guest = host;
            guest.rotateSeats(offset);
            auto guestSeat = [&](int seat) { return (seat - offset + players) % players; };

            int guard = 0;
            while (!host.matchOver() && ++guard < 20000) {
                std::string error;
                CHECK(host.validate(&error));
                CHECK(rotated(host, offset) == guest.serializeState());
                {
                    MultiCore copy;
                    CHECK(copy.restoreState(host.serializeState()));
                    CHECK(copy.serializeState() == host.serializeState());
                }
                if (host.roundOver()) {
                    ++rounds;
                    contracts[static_cast<int>(host.contract())]++;
                    made += host.endReason() == MultiEndReason::ContractMade;
                    failed += host.endReason() == MultiEndReason::ContractFailed;
                    claims += host.endReason() == MultiEndReason::Claim66;
                    lastTricks += host.endReason() == MultiEndReason::LastTrick;
                    doubled += host.doubling() > 1;
                    sitOuts += host.sittingOut() >= 0;
                    CHECK(host.roundWinnerMask() != 0);
                    CHECK(host.roundAward() >= 1);
                    CHECK(host.nextRound());
                    CHECK(guest.nextRound());
                    continue;
                }
                if (host.trickPending()) {
                    const int w = host.commitTrick();
                    const int gw = guest.commitTrick();
                    CHECK(w >= 0 && gw == guestSeat(w));
                    continue;
                }
                // Claims can come from any seat; give them a chance first.
                bool claimed = false;
                for (int seat = 0; seat < players && !claimed; ++seat) {
                    if (host.canClaim(seat) && pick() % 3 == 0) {
                        CHECK(host.apply(seat, {MultiActionType::Claim, -1, false}));
                        CHECK(guest.apply(guestSeat(seat), {MultiActionType::Claim, -1, false}));
                        claimed = true;
                    }
                }
                if (claimed)
                    continue;
                const int actor = host.actor();
                CHECK(actor >= 0);
                if (actor < 0)
                    break;
                if (host.phase() == MultiPhase::Doubling && host.variant() == MultiVariant::HungarianFour) {
                    // The order of who is asked must not depend on the secret partner.
                    CHECK(actor != host.declarer() || host.doublingStep() % 2 == 1);
                }
                const auto legal = host.legalActions(actor);
                CHECK(!legal.empty());
                const auto guestLegal = guest.legalActions(guestSeat(actor));
                CHECK(legal.size() == guestLegal.size());
                // Ask the AI on a copy so the host's RNG stays identical to
                // the guest's; apply() itself must be deterministic.
                MultiCore thinker = host;
                const MultiAction action = (pick() % 4 == 0) ? legal[pick() % legal.size()]
                                                             : thinker.chooseAiAction(actor);
                CHECK(host.isLegal(actor, action));
                CHECK(host.apply(actor, action));
                CHECK(guest.apply(guestSeat(actor), action));
                ++actions;
            }
            CHECK(guard < 20000);
            CHECK(host.matchWinnerMask() != 0);
        }
        std::printf("%-16s rounds %6ld actions %8ld | N %d B %d S %d G %d Z %d KS %d BS %d KBS %d | made %d failed %d claim %d last %d doubled %d sitout %d\n",
                    names[v], rounds, actions, contracts[0], contracts[1], contracts[2], contracts[3],
                    contracts[4], contracts[5], contracts[6], contracts[7], made, failed, claims, lastTricks,
                    doubled, sitOuts);
    }
    if (failures) {
        std::printf("%d failures\n", failures);
        return 1;
    }
    std::printf("OK\n");
    return 0;
}
