#pragma once

#include <cstdint>
#include <type_traits>

// Axis-aligned bounding-box collision detection over a caller-owned
// Entity, and nothing else (collision-system spec A7/NFR-12: pure
// geometry, zero I/O, zero rendering surface). No steamcore/ header is
// included -- not even config.h -- because this primitive is not bounded
// by or clipped to the 240x160 screen (spec A6): coordinates are plain
// screen-space int32_t pixels that MAY be negative and MAY lie entirely
// off-screen -- unlike Framebuffer's drawing calls, nothing here clips.
//
// Entity is a four-field POD, deliberately minimal (spec A1): no ID, no
// velocity, no sprite reference. A game that needs more composes Entity
// as a member of its own richer struct rather than this type growing a
// fifth field. Each Entity describes the half-open region
// [x, x+w) x [y, y+h) -- the right and bottom edges are exclusive.
//
// Touching is NOT colliding (spec A4): given a{x:0, w:10} and
// b{x:10, w:10}, a's right edge (x=10) equals b's left edge (x=10), and
// overlaps(a, b) is false. Any entity with w <= 0 or h <= 0 overlaps
// nothing at all, including another such entity -- mirroring
// Framebuffer::fillRect's non-positive-size no-op (spec A5). overlaps()
// is proven overflow-free at every int32_t input, including the extremes
// (spec A6): every coordinate is widened to int64_t before any addition,
// never after -- clip.h's widen-before-summing idiom, made constexpr so
// the extreme cases are additionally proven at compile time, where a
// 32-bit narrowing back into UB is a hard build error rather than a
// finding.
//
// checkCollision()'s callback is invoked as callback(a, b), always in
// that fixed order; checkCollision() itself holds no state and writes
// neither entity -- a callback that mutates one is the caller's own
// choice. A callback whose parameters are declared by value still
// compiles but then mutates a throwaway copy, never the caller's real
// entity -- the same by-value footgun game_loop.h's review F10 already
// records for render(Framebuffer).
//
// Contract: single-threaded, nothing throws, no error code, no dynamic
// allocation -- same inherited contract as every other steamcore type.
//
// Example (bullet-vs-enemy inside a game's update, game_loop.h/input.h
// style):
//   void update(const steamcore::GameInput& input) {
//     steamcore::checkCollision(bullet_, enemy_, [](Entity& a, Entity& b) {
//       a.w = 0;  // bullet consumed
//       b.w = 0;  // enemy destroyed
//     });
//   }

namespace steamcore {

struct Entity {
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
};

static_assert(sizeof(Entity) == 4 * sizeof(int32_t),
              "Entity must carry exactly its four named fields");
static_assert(std::is_standard_layout_v<Entity>,
              "Entity must stay a plain POD -- no virtual member, no base "
              "class");

// Reports whether the two half-open regions [a.x, a.x+a.w) x
// [a.y, a.y+a.h) and [b.x, b.x+b.w) x [b.y, b.y+b.h) overlap. Touching
// edges do not overlap; a zero- or negative-size entity overlaps nothing.
constexpr bool overlaps(const Entity& a, const Entity& b) {
  if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0) return false;
  const int64_t ax0 = a.x, ay0 = a.y, bx0 = b.x, by0 = b.y;
  const int64_t ax1 = ax0 + a.w, ay1 = ay0 + a.h;  // exclusive
  const int64_t bx1 = bx0 + b.w, by1 = by0 + b.h;  // exclusive
  return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

// Invokes `callback(a, b)` exactly once, in that fixed order, if and only
// if `overlaps(a, b)` is true. Holds no state itself and writes neither
// entity; a callback that mutates one is the caller's own choice. A
// callback whose parameters are declared by value still compiles but
// then mutates a throwaway copy -- the same by-value warning game_loop.h
// records for render() (review F10).
template <typename Callback>
void checkCollision(Entity& a, Entity& b, Callback&& callback) {
  static_assert(
      std::is_invocable_v<Callback&, Entity&, Entity&>,
      "Callback must be callable as callback(steamcore::Entity&, "
      "steamcore::Entity&)");
  if (overlaps(a, b)) callback(a, b);
}

// Checks every distinct unordered pair in `entities[0..count)` exactly
// once -- never a pair against itself -- delegating each pair to
// `checkCollision` so the sweep and the pair test can never disagree.
// `entities == nullptr` or `count <= 0` is a no-op, not a crash; `count
// == 1` never fires either, since there is no second entity to pair it
// with. No allocation: iterates the caller-owned array in place.
//
// Each entity is read at the point its own pair is tested, so a callback
// that mutates an entity changes the outcome of every pair tested after
// it within the same sweep -- documented, not prevented: collision rules
// are the caller's business (plan R7).
template <typename Callback>
void sweepCollisions(Entity* entities, int32_t count, Callback&& callback) {
  if (entities == nullptr || count <= 0) return;
  for (int32_t i = 0; i < count; ++i) {
    for (int32_t j = i + 1; j < count; ++j) {
      checkCollision(entities[i], entities[j], callback);
    }
  }
}

}  // namespace steamcore
