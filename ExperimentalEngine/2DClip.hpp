#pragma once
#include <glm/glm.hpp>

enum class ClipCode : unsigned int {
	Inside = 0,
	Left = 1,
	Right = 2,
	Bottom = 4,
	Top = 8
};

inline ClipCode operator |(ClipCode lhs, ClipCode rhs) {
	return static_cast<ClipCode> (
		static_cast<unsigned>(lhs) |
		static_cast<unsigned>(rhs)
		);
}

inline ClipCode operator &(ClipCode lhs, ClipCode rhs) {
	return static_cast<ClipCode> (
		static_cast<unsigned>(lhs) &
		static_cast<unsigned>(rhs)
		);
}

inline bool hasFlag(ClipCode code, ClipCode flag) {
	return (code & flag) == flag;
}

inline ClipCode calcClipCode(glm::vec2 point, glm::vec2 min, glm::vec2 max) {
	ClipCode code;

	code = ClipCode::Inside;          // initialised as being inside of [[clip window]]

	if (point.x < min.x)           // to the left of clip window
		code = code | ClipCode::Left;
	else if (point.x > max.x)      // to the right of clip window
		code = code | ClipCode::Right;
	if (point.y < min.y)           // below the clip window
		code = code | ClipCode::Bottom;
	else if (point.y > max.y)      // above the clip window
		code = code | ClipCode::Top;

	return code;
}

// Cohen–Sutherland clipping algorithm clips a line from
// P0 = (x0, y0) to P1 = (p1.x, y1) against a rectangle with 
// diagonal from (min.x, ymin) to (max.x, ymax).
inline bool lineClip(glm::vec2& p0, glm::vec2& p1, glm::vec2 min, glm::vec2 max) {
	// compute outcodes for P0, P1, and whatever point lies outside the clip rectangle
	ClipCode ccode0 = calcClipCode(p0, min, max);
	ClipCode ccode1 = calcClipCode(p1, min, max);
	bool accept = false;

	while (true) {
		if (!(int)(ccode0 | ccode1)) {
			// bitwise OR is 0: both points inside window; trivially accept and exit loop
			accept = true;
			break;
		} else if ((int)(ccode0 & ccode1)) {
			// bitwise AND is not 0: both points share an outside zone (LEFT, RIGHT, TOP,
			// or BOTTOM), so both must be outside window; exit loop (accept is false)
			break;
		} else {
			// failed both tests, so calculate the line segment to clip
			// from an outside point to an intersection with clip edge
			float x, y;

			// At least one endpoint is outside the clip rectangle; pick it.
			ClipCode outcodeOut = ccode1 > ccode0 ? ccode1 : ccode0;

			// Now find the intersection point;
			// use formulas:
			//   slope = (y1 - y0) / (p1.x - p0.x)
			//   x = p0.x + (1 / slope) * (ym - y0), where ym is ymin or ymax
			//   y = y0 + slope * (xm - p0.x), where xm is min.x or max.x
			// No need to worry about divide-by-zero because, in each case, the
			// outcode bit being tested guarantees the denominator is non-zero
			if (hasFlag(outcodeOut, ClipCode::Top)) {           // point is above the clip window
				x = p0.x + (p1.x - p0.x) * (max.y - p0.y) / (p1.y - p0.y);
				y = max.y;
			} else if (hasFlag(outcodeOut, ClipCode::Bottom)) { // point is below the clip window
				x = p0.x + (p1.x - p0.x) * (min.y - p0.y) / (p1.y - p0.y);
				y = min.y;
			} else if (hasFlag(outcodeOut, ClipCode::Right)) {  // point is to the right of clip window
				y = p0.y + (p1.y - p0.y) * (max.x - p0.x) / (p1.x - p0.x);
				x = max.x;
			} else if (hasFlag(outcodeOut, ClipCode::Left)) {   // point is to the left of clip window
				y = p0.y + (p1.y - p0.y) * (min.x - p0.x) / (p1.x - p0.x);
				x = min.x;
			}

			// Now we move outside point to intersection point to clip
			// and get ready for next pass.
			if (outcodeOut == ccode0) {
				p0.x = x;
				p0.y = y;
				ccode0 = calcClipCode(p0, min, max);
			} else {
				p1.x = x;
				p1.y = y;
				ccode1 = calcClipCode(p1, min, max);
			}
		}
	}

	return accept;
}
