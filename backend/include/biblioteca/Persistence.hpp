#pragma once
#include <stdexcept>
#include <string>
namespace biblioteca {
enum class WriteOutcome { confirmado, rechazado, incierto };
inline std::string outcomeName(WriteOutcome value) {
    switch (value) {
    case WriteOutcome::confirmado: return "confirmado";
    case WriteOutcome::rechazado: return "rechazado";
    case WriteOutcome::incierto: return "incierto";
    }
    return "incierto";
}
class PersistenceError : public std::runtime_error {
public:
    PersistenceError(WriteOutcome result, const std::string& detail) : std::runtime_error(detail), outcome(result) {}
    WriteOutcome outcome;
};
} // namespace biblioteca
