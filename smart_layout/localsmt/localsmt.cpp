#include "localsmt.h"
#include "nia_ls.h"
#include "ration_num.h"
#include <cmath>
#include <algorithm>
#include <set>
#include <sstream>

namespace localsmt {

// ─── Expr ───────────────────────────────────────────────────────────────────

Expr::Expr(double c) : constant_(c) {}

Expr::Expr(const std::string& var_name, Engine* engine)
    : constant_(0.0), engine_(engine), primary_name_(var_name) {
    terms_.push_back({var_name, 1.0});
}

Expr Expr::operator+(const Expr& o) const {
    Expr r = *this;
    r.constant_ += o.constant_;
    r.primary_name_.clear();  // no longer a simple variable
    for (auto& t : o.terms_) {
        bool found = false;
        for (auto& rt : r.terms_) {
            if (rt.var_name == t.var_name) {
                rt.coeff += t.coeff;
                found = true;
                break;
            }
        }
        if (!found) r.terms_.push_back(t);
    }
    // propagate engine pointer
    if (!r.engine_) r.engine_ = o.engine_;
    return r;
}

Expr Expr::operator-(const Expr& o) const { return *this + (-o); }

Expr Expr::operator-() const { return (*this) * (-1.0); }

Expr Expr::operator*(double s) const {
    Expr r;
    r.constant_ = constant_ * s;
    for (auto& t : terms_)
        r.terms_.push_back({t.var_name, t.coeff * s});
    r.engine_ = engine_;
    return r;
}

Expr operator*(double s, const Expr& e) { return e * s; }
Expr operator+(double c, const Expr& e) { return Expr(c) + e; }
Expr operator-(double c, const Expr& e) { return Expr(c) - e; }

// Comparison → Constraint
Constraint Expr::operator<=(const Expr& rhs) const { return Constraint(*this, LEQ, rhs); }
Constraint Expr::operator>=(const Expr& rhs) const { return Constraint(*this, GEQ, rhs); }
Constraint Expr::operator==(const Expr& rhs) const { return Constraint(*this, EQ, rhs); }
// Strict operators in integer domain.
// Strict inequality uses integer-domain mapping:
// a < b => a <= b - 1, a > b => a >= b + 1.
Constraint Expr::operator<(const Expr& rhs) const { return Constraint(*this, LEQ, rhs - Expr(1.0)); }
Constraint Expr::operator>(const Expr& rhs) const { return Constraint(*this, GEQ, rhs + Expr(1.0)); }
Constraint Expr::operator<=(double rhs) const { return Constraint(*this, LEQ, Expr(rhs)); }
Constraint Expr::operator>=(double rhs) const { return Constraint(*this, GEQ, Expr(rhs)); }
Constraint Expr::operator==(double rhs) const { return Constraint(*this, EQ, Expr(rhs)); }
Constraint Expr::operator<(double rhs) const { return Constraint(*this, LEQ, Expr(rhs - 1.0)); }
Constraint Expr::operator>(double rhs) const { return Constraint(*this, GEQ, Expr(rhs + 1.0)); }

double Expr::value() const {
    if (!engine_) throw std::runtime_error("Expr not bound to an engine");
    if (primary_name_.empty()) throw std::runtime_error("value() only works on simple variable Expr");
    return engine_->getVariable(primary_name_);
}

const std::string& Expr::name() const { return primary_name_; }

// ─── Constraint ─────────────────────────────────────────────────────────────

Constraint::Constraint(const Expr& l, CompOp o, const Expr& r)
    : lhs(l), op(o), rhs(r) {}

// ─── Engine ─────────────────────────────────────────────────────────────────

Engine::Engine() {}
Engine::~Engine() {}

void Engine::ensureVar(const std::string& name) {
    if (var_index_.find(name) == var_index_.end()) {
        var_index_[name] = (int)var_names_.size();
        var_names_.push_back(name);
    }
}

Expr Engine::var(const std::string& name) {
    ensureVar(name);
    return Expr(name, this);
}

Expr Engine::var(const std::string& name, double initial) {
    ensureVar(name);
    var_initial_[name] = initial;
    return Expr(name, this);
}

Expr Engine::var(const std::string& name, double lo, double hi) {
    ensureVar(name);
    var_bounds_[name] = {lo, hi};
    return Expr(name, this);
}

Expr Engine::var(const std::string& name, double lo, double hi, double initial) {
    ensureVar(name);
    var_bounds_[name] = {lo, hi};
    var_initial_[name] = initial;
    return Expr(name, this);
}

LitRef Engine::add(const Constraint& c, int strength) {
    // Ensure all variables in lhs/rhs are registered
    for (auto& t : c.lhs.terms_) ensureVar(t.var_name);
    for (auto& t : c.rhs.terms_) ensureVar(t.var_name);

    int id = next_id_++;
    ConstraintInfo ci;
    ci.lhs = c.lhs;
    ci.op = c.op;
    ci.rhs = c.rhs;
    ci.strength = strength;
    ci.lit_id = id;
    ci.removed = false;

    id_to_idx_[id] = constraints_.size();
    constraints_.push_back(ci);
    dirty_ = true;
    return {id};
}

void Engine::addClause(const std::vector<LitRef>& lits) {
    std::vector<int> ids;
    for (auto& l : lits) ids.push_back(l.id);
    clauses_.push_back(ids);
    dirty_ = true;
}

void Engine::removeConstraint(LitRef lit) {
    auto it = id_to_idx_.find(std::abs(lit.id));
    if (it != id_to_idx_.end() && it->second < constraints_.size()) {
        constraints_[it->second].removed = true;
        dirty_ = true;
    }
}

// ─── Inspection ────────────────────────────────────────────────────────────

std::string Engine::exprToString(const Expr& e) {
    std::ostringstream os;
    bool first = true;
    for (auto& t : e.terms_) {
        if (t.coeff == 0) continue;
        if (!first) {
            if (t.coeff > 0) os << " + ";
            else { os << " - "; }
        } else if (t.coeff < 0) {
            os << "-";
        }
        first = false;
        double absC = std::abs(t.coeff);
        if (absC != 1.0) os << absC << "*";
        os << t.var_name;
    }
    if (e.constant_ != 0 || first) {
        if (!first && e.constant_ > 0) os << " + ";
        else if (!first && e.constant_ < 0) os << " - ";
        if (!first && e.constant_ < 0) os << std::abs(e.constant_);
        else os << e.constant_;
    }
    return os.str();
}

std::vector<Engine::ConstraintDesc> Engine::getConstraints() const {
    std::vector<ConstraintDesc> result;
    result.reserve(constraints_.size());
    for (auto& ci : constraints_) {
        const char* opStr = "==";
        if (ci.op == LEQ) opStr = "<=";
        else if (ci.op == GEQ) opStr = ">=";

        std::ostringstream os;
        os << exprToString(ci.lhs) << " " << opStr << " " << exprToString(ci.rhs);

        result.push_back({ci.lit_id, os.str(), ci.strength, ci.removed});
    }
    return result;
}

std::vector<std::vector<int>> Engine::getClauses() const {
    return clauses_;
}

std::vector<std::string> Engine::getVariableNames() const {
    return var_names_;
}

void Engine::clear() {
    constraints_.clear();
    clauses_.clear();
    id_to_idx_.clear();
    var_names_.clear();
    var_index_.clear();
    var_bounds_.clear();
    var_initial_.clear();
    solution_.clear();
    next_id_ = 1;
    solved_ = false;
    dirty_ = true;
}

// ─── Direct Build & Solve ───────────────────────────────────────────────────

// Helper: normalize Expr to terms + constant (combine like terms)
struct NormExpr {
    struct Term { std::string var; ration_num coeff; };
    std::vector<Term> terms;
    ration_num constant;
};

// normalize is called from Engine::buildAndSolve (Engine is friend of Expr)
static NormExpr normalizeExprs(const std::vector<Expr::Term>& lhs_terms, double lhs_const,
                               const std::vector<Expr::Term>& rhs_terms, double rhs_const) {
    NormExpr result;
    std::map<std::string, double> combined;
    for (auto& t : lhs_terms) combined[t.var_name] += t.coeff;
    for (auto& t : rhs_terms) combined[t.var_name] -= t.coeff;
    double c = lhs_const - rhs_const;

    for (auto& kv : combined) {
        ration_num rc = ration_num::from_double(kv.second);
        if (rc != 0) result.terms.push_back({kv.first, rc});
    }
    result.constant = ration_num::from_double(c);
    return result;
}

bool Engine::buildAndSolve(int width, int height) {
    nia_overall::ls_solver solver(0, 50000, 10, true);

    // ── 1. Collect active hard constraints ──
    struct ActiveConstraint {
        size_t idx;
        int user_id;
        int solver_lit_id;
    };
    std::vector<ActiveConstraint> active;
    int next_lit = 1;

    // localsmt currently solves hard constraints only.
    // Non-zero strengths are accepted for API compatibility but are not
    // optimized as weighted soft constraints.
    for (size_t i = 0; i < constraints_.size(); i++) {
        if (constraints_[i].removed) continue;
        active.push_back({i, constraints_[i].lit_id, next_lit++});
    }

    // Bound constraints
    struct BoundLit {
        std::string var;
        bool is_leq;
        ration_num bound;
        int solver_lit_id;
    };
    std::vector<BoundLit> bound_lits;
    for (auto& kv : var_bounds_) {
        if (kv.second.lo > -1e17)
            bound_lits.push_back({kv.first, false, ration_num::from_double(kv.second.lo), next_lit++});
        if (kv.second.hi < 1e17)
            bound_lits.push_back({kv.first, true, ration_num::from_double(kv.second.hi), next_lit++});
    }

    int total_lits = next_lit - 1;

    // ── 2. Allocate solver lits ──
    solver.make_lits_space(total_lits + 1);
    solver._lits[0].lits_index = 0;

    // ── 3. Register variables ──
    for (auto& name : var_names_)
        solver.transfer_name_to_var(name, true);
    solver.transfer_name_to_var(".w", true);
    solver.transfer_name_to_var("BC_hight", true);

    // ── 4. Build constraint literals ──
    std::map<int, int> user_to_solver;

    for (auto& ac : active) {
        auto& ci = constraints_[ac.idx];
        user_to_solver[ac.user_id] = ac.solver_lit_id;

        nia_overall::lit& l = solver._lits[ac.solver_lit_id];
        l.is_nia_lit = true;
        l.lits_index = ac.solver_lit_id;

        NormExpr ne = normalizeExprs(ci.lhs.terms_, ci.lhs.constant_,
                                      ci.rhs.terms_, ci.rhs.constant_);

        for (auto& t : ne.terms) {
            uint64_t vi = solver.name2var[t.var];
            l.coff_vars.push_back(nia_overall::coff_var((int)vi, t.coeff));
        }

        // Internal: key + sum(coff*var) <= 0 (or == 0)
        switch (ci.op) {
            case LEQ:
                l.key = ne.constant;
                l.is_equal = false;
                break;
            case GEQ:
                for (auto& cv : l.coff_vars) cv.coff = -cv.coff;
                l.key = -ne.constant;
                l.is_equal = false;
                break;
            case EQ:
                l.key = ne.constant;
                l.is_equal = true;
                break;
        }
        solver._num_opt += l.coff_vars.size();
    }

    // ── 5. Build bound literals ──
    for (auto& bl : bound_lits) {
        nia_overall::lit& l = solver._lits[bl.solver_lit_id];
        l.is_nia_lit = true;
        l.lits_index = bl.solver_lit_id;
        uint64_t vi = solver.name2var[bl.var];
        if (bl.is_leq) {
            l.coff_vars.push_back(nia_overall::coff_var((int)vi, ration_num(1)));
            l.key = -bl.bound;
        } else {
            l.coff_vars.push_back(nia_overall::coff_var((int)vi, ration_num(-1)));
            l.key = bl.bound;
        }
        l.is_equal = false;
        solver._num_opt += 1;
    }

    // ── 6. Build clauses ──
    std::set<int> in_user_clause;
    for (auto& cl : clauses_)
        for (int uid : cl)
            in_user_clause.insert(std::abs(uid));

    auto& ov = solver.original_vec;

    for (auto& ac : active) {
        if (in_user_clause.count(ac.user_id)) continue;
        ov.push_back({ac.solver_lit_id});
    }

    for (auto& cl : clauses_) {
        std::vector<int> solver_cl;
        for (int uid : cl) {
            int sign = (uid > 0) ? 1 : -1;
            auto it = user_to_solver.find(std::abs(uid));
            if (it != user_to_solver.end())
                solver_cl.push_back(sign * it->second);
        }
        if (!solver_cl.empty()) ov.push_back(solver_cl);
    }

    for (auto& bl : bound_lits)
        ov.push_back({bl.solver_lit_id});

    solver.delete_redundant_clauses(ov);

    // ── 7. Finalize solver ──
    solver.basic_component_name = "BC";
    solver.bc_width_idx = (int)solver.name2var[".w"];
    solver.bc_hight_idx = (int)solver.name2var["BC_hight"];
    solver._num_vars = solver._vars.size();
    solver.lit_appear.resize(solver._num_lits);
    solver.lits_in_cls = new Array((int)solver._num_lits);
    solver.prepare_components_idx();
    solver.record_info_after_read_file();

    // ── 7b. Apply initial values ──
    for (auto& kv : var_initial_) {
        auto it = solver.name2var.find(kv.first);
        if (it != solver.name2var.end() && it->second < solver._solution.size()) {
            solver._solution[it->second] = ration_num::from_double(kv.second);
        }
    }

    // ── 8. Solve ──
    solver.build_instance_original();
    if (width > 0 || height > 0)
        solver.build_instance_new_width(width, height);

    bool ok = solver.assume_one_literal() || solver.local_search();
    if (!ok) { solved_ = false; return false; }

    // ── 9. Extract solution ──
    solution_.resize(var_names_.size(), 0.0);
    for (size_t i = 0; i < solver._vars.size() && i < solver._solution.size(); i++) {
        auto it = var_index_.find(solver._vars[i].var_name);
        if (it != var_index_.end())
            solution_[it->second] = solver._solution[i].to_double();
    }

    solved_ = true;
    dirty_ = false;
    return true;
}

bool Engine::solve(int width, int height) { return buildAndSolve(width, height); }
bool Engine::resolve(int width, int height) { return buildAndSolve(width, height); }

std::vector<Engine::VarResult> Engine::getResults() const {
    std::vector<VarResult> results;
    if (!solved_) return results;
    for (size_t i = 0; i < var_names_.size(); i++)
        results.push_back({var_names_[i], solution_[i]});
    return results;
}

double Engine::getVariable(const std::string& name) const {
    if (!solved_) return 0.0;
    auto it = var_index_.find(name);
    if (it == var_index_.end()) return 0.0;
    return solution_[it->second];
}

} // namespace localsmt
