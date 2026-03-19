#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <stdexcept>

namespace localsmt {

// ── Strength ────────────────────────────────────────────────────────────────

enum Strength {
    REQUIRED = 0,
    WEAK     = 1,
    MEDIUM   = 100,
    STRONG   = 1000,
};

// ── Forward declarations ────────────────────────────────────────────────────

class Engine;
class Expr;
class Constraint;

// ── Expr: a linear expression over solver variables ─────────────────────────
//
// Supports arithmetic: +, -, *, unary -
// Supports comparison: <=, >=, == → returns Constraint
//
// Example:
//   Expr x = e.var("x");
//   Expr y = e.var("y");
//   e.add(2 * x + y - 10 <= 3 * y);

class Expr {
public:
    struct Term {
        std::string var_name;
        double coeff;
    };

    Expr() = default;
    Expr(double c);  // constant

    // Arithmetic
    Expr operator+(const Expr& o) const;
    Expr operator-(const Expr& o) const;
    Expr operator-() const;
    Expr operator*(double s) const;
    friend Expr operator*(double s, const Expr& e);
    friend Expr operator+(double c, const Expr& e);
    friend Expr operator-(double c, const Expr& e);

    // Comparison → Constraint
    // Strict operators are interpreted in integer domain:
    // a < b  => a <= b - 1
    // a > b  => a >= b + 1
    Constraint operator<=(const Expr& rhs) const;
    Constraint operator>=(const Expr& rhs) const;
    Constraint operator==(const Expr& rhs) const;
    Constraint operator<(const Expr& rhs) const;
    Constraint operator>(const Expr& rhs) const;
    Constraint operator<=(double rhs) const;
    Constraint operator>=(double rhs) const;
    Constraint operator==(double rhs) const;
    Constraint operator<(double rhs) const;
    Constraint operator>(double rhs) const;

    /// Get solved value (only valid after Engine::solve)
    double value() const;

    /// Variable name (empty if this is a complex expression)
    const std::string& name() const;

private:
    friend class Engine;

    // Constructor for named variable (only Engine can create)
    Expr(const std::string& var_name, Engine* engine);

    std::vector<Term> terms_;
    double constant_ = 0.0;
    Engine* engine_ = nullptr;       // back-pointer for value()
    std::string primary_name_;       // set for simple variable exprs
};

// ── Constraint: result of comparing two Expr ────────────────────────────────

enum CompOp { LEQ, GEQ, EQ };

class Constraint {
public:
    Constraint() = default;

    Expr lhs;
    CompOp op = EQ;
    Expr rhs;

private:
    friend class Expr;
    Constraint(const Expr& l, CompOp o, const Expr& r);
};

// ── LitRef: handle returned by Engine::add() for use in clauses ─────────────

struct LitRef {
    int id = 0;
    LitRef operator-() const { return {-id}; }
    operator int() const { return id; }
};

// ── Engine ──────────────────────────────────────────────────────────────────
//
// Usage:
//   Engine e;
//   Expr x = e.var("x", 0, 1000);
//   Expr y = e.var("y");
//   e.add(x + y <= 100);
//   e.add(x == 50, WEAK);
//   auto c1 = e.add(x <= 30);
//   auto c2 = e.add(y >= 20);
//   e.addClause({c1, -c2});
//   e.solve(800, 600);
//   double xv = x.value();

class Engine {
public:
    Engine();
    ~Engine();

    // ── Variables ──

    /// Create a variable expression (no bounds)
    Expr var(const std::string& name);

    /// Create a variable expression with initial value (no bounds)
    Expr var(const std::string& name, double initial);

    /// Create a variable expression with bounds
    Expr var(const std::string& name, double lo, double hi);

    /// Create a variable expression with bounds and initial value
    Expr var(const std::string& name, double lo, double hi, double initial);

    // ── Constraints ──

    /// Add a constraint. Returns a literal handle for use in clauses.
    /// localsmt currently solves all active constraints as hard constraints.
    /// `strength` is kept for compatibility/inspection only.
    LitRef add(const Constraint& c, int strength = REQUIRED);

    /// Add a disjunctive clause: at least one literal must hold.
    /// Use -lit for negation.
    void addClause(const std::vector<LitRef>& lits);

    // ── Remove ──

    void removeConstraint(LitRef lit);

    // ── Solve ──

    bool solve(int width = 0, int height = 0);
    bool resolve(int width, int height);

    // ── Results ──

    struct VarResult {
        std::string name;
        double value;
    };

    std::vector<VarResult> getResults() const;
    double getVariable(const std::string& name) const;
    bool isSolved() const { return solved_; }

    // ── Inspection ──

    /// Description of a constraint for debugging / inspection
    struct ConstraintDesc {
        int id;                 // LitRef id
        std::string expr;      // human-readable, e.g. "x + 2*y <= 100"
        int strength;          // REQUIRED / WEAK / MEDIUM / STRONG or custom
        bool removed;          // true if removeConstraint() was called
    };

    /// Return descriptions of all added constraints
    std::vector<ConstraintDesc> getConstraints() const;

    /// Return descriptions of all added clauses (each clause is a list of literal ids)
    std::vector<std::vector<int>> getClauses() const;

    /// Return all registered variable names
    std::vector<std::string> getVariableNames() const;

    // ── Utility ──

    void clear();

private:
    friend class Expr;

    struct ConstraintInfo {
        Expr lhs;
        CompOp op;
        Expr rhs;
        int strength;
        int lit_id;
        bool removed;
    };

    std::map<int, size_t> id_to_idx_;
    std::vector<ConstraintInfo> constraints_;
    std::vector<std::vector<int>> clauses_;

    std::vector<std::string> var_names_;
    std::map<std::string, int> var_index_;
    struct VarBound { double lo = -1e18; double hi = 1e18; };
    std::map<std::string, VarBound> var_bounds_;
    std::map<std::string, double> var_initial_;   // initial values for solver hint

    int next_id_ = 1;
    bool solved_ = false;
    bool dirty_ = true;

    std::vector<double> solution_;

    static std::string exprToString(const Expr& e);
    bool buildAndSolve(int width, int height);
    void ensureVar(const std::string& name);
};

} // namespace localsmt
