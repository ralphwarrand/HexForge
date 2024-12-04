#pragma once

#include <glm/glm.hpp>
#include <format>

namespace Hex
{
	struct Position
	{
	    glm::vec3 value; // Store the position vector

		// Default constructor
		Position() : value(0.0f, 0.0f, 0.0f) {}

		// Constructor for easy initialization
		Position(const float& x, const float& y, const float& z) : value(x, y, z) {}

		// Allow initialization with glm::vec3 directly
	    explicit Position(const glm::vec3& vec) : value(vec) {}

		// Access individual components with `x`, `y`, `z`
		float& x() { return value.x; }
		float& y() { return value.y; }
		float& z() { return value.z; }

		const float& x() const { return value.x; }
		const float& y() const { return value.y; }
		const float& z() const { return value.z; }

		// Overload the stream operator for easy printing
		friend std::ostream& operator<<(std::ostream& os, const Position& pos) {
			return os << "Position: (" << pos.x() << ", " << pos.y() << ", " << pos.z() << ")";
		}
	};

	struct Velocity
	{
		glm::vec3 value; // Store the position vector

		// Default constructor
		Velocity() : value(0.0f, 0.0f, 0.0f) {}

		// Constructor for easy initialization
		Velocity(const float& x, const float& y, const float& z) : value(x, y, z) {}

		// Allow initialization with glm::vec3 directly
		explicit Velocity(const glm::vec3& vec) : value(vec) {}

		// Access individual components with `x`, `y`, `z`
		float& x() { return value.x; }
		float& y() { return value.y; }
		float& z() { return value.z; }

		const float& x() const { return value.x; }
		const float& y() const { return value.y; }
		const float& z() const { return value.z; }

		// Overload the stream operator for easy printing
		friend std::ostream& operator<<(std::ostream& os, const Velocity& vel) {
			return os << "Velocity: (" << vel.x() << ", " << vel.y() << ", " << vel.z() << ")";
		}
	};
}


// Specialize std::formatter for Hex::Position in the std namespace
template <>
struct std::formatter<Hex::Position> : std::formatter<std::string> {
	auto format(const Hex::Position& pos, std::format_context& ctx) const { // Add const qualifier here
		return std::formatter<std::string>::format(
			std::format("({:.2f}, {:.2f}, {:.2f})", pos.x(), pos.y(), pos.z()),
			ctx
		);
	}
};