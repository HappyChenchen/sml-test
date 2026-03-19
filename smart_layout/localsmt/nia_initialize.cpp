#include "nia_ls.h"
namespace nia_overall
{

    // initialize
    ls_solver::ls_solver()
        : _swt_p(0.3),
          _swt_threshold(50),
          smooth_probability(3),
          _cutoff(1200),
          _additional_len(10),
          _max_step(UINT64_MAX),
          _complete_ls(false)
    {
        mt.seed(1);
    }

    ls_solver::ls_solver(int random_seed, uint64_t max_step, double cutoff, bool complete_ls)
        : _swt_p(0.3),
          _swt_threshold(50),
          smooth_probability(3),
          _cutoff(cutoff),
          _additional_len(10),
          _max_step(max_step),
          _complete_ls(complete_ls)
    {
        mt.seed(random_seed);
    }

    void ls_solver::initialize()
    {
        prepare_cls_lit_idx_for_vars();
        set_pre_value();
        clear_prev_data();
        construct_solution();
        initialize_clause_datas();
        initialize_variable_datas();
        best_found_this_restart = unsat_clauses->size();
        update_best_solution();
    }
    void ls_solver::clear_prev_data()
    {
        for (int v : bool_var_vec)
        {
            _vars[v].score = 0;
        }
        _best_found_hard_cost_this_bool = INT32_MAX;
        _best_found_hard_cost_this_nia = INT32_MAX;
        no_improve_cnt_bool = 0;
        no_improve_cnt_nia = 0;
        unsat_clauses->clear();
        std::fill(tabulist.begin(), tabulist.end(), 0);
        std::fill(_lit_make_break.begin(), _lit_make_break.end(), 0);
        std::fill(is_in_unsat_clause.begin(), is_in_unsat_clause.end(), false);
        std::fill(last_move.begin(), last_move.end(), 0);
        std::fill(is_chosen_bool_var.begin(), is_chosen_bool_var.end(), false);
        false_lit_occur->clear();
    }

    // construction
    void ls_solver::construct_solution()
    {
        static bool first_into = true;
        for (int i = 0; i < _num_vars; i++)
        {
            ration_num var_value;
            if (first_into)
                var_value = (_vars[i].is_nia) ? 0 : -1;
            else
                var_value = _solution[i];
            if (!_vars[i].is_nia)
            {
                if (up_value_vars[i] != 0)
                    _solution[i] = up_value_vars[i];
                else
                    _solution[i] = var_value;
                continue;
            }
            if (_vars[i].low_bound > var_value)
                _solution[i] = _vars[i].low_bound;
            else if (_vars[i].upper_bound < var_value)
                _solution[i] = _vars[i].upper_bound;
            else
                _solution[i] = var_value;
        }
        initialize_lit_datas();
        first_into = false;
    }

    void ls_solver::read_model()
    {
        int size;
        std::cin >> size;
        std::string in_string, in_string_2;
        for (int i = 0; i < size; i++)
        {
            std::cin >> in_string;
            std::cin >> in_string_2;
            if (name2var.find(in_string) != name2var.end())
            {
                if (in_string_2 == "false")
                {
                    _solution[name2var[in_string]] = -1;
                }
                else if (in_string_2 == "true")
                {
                    _solution[name2var[in_string]] = 1;
                }
                else
                {
                    _solution[name2var[in_string]] = atoi(in_string_2.c_str());
                }
            }
        }
    }

    void ls_solver::initialize_variable_datas()
    {
    }

    // initialize the delta of each literal by delta_lit operation
    void ls_solver::initialize_lit_datas()
    {
        for (lit &l : _lits)
        {
            if (l.is_nia_lit)
            {
                if (l.lits_index == 0)
                {
                    l.is_true = true;
                } // the nia lit has been asserted as true
                else
                {
                    l.delta = delta_lit(l);
                    if (l.is_equal)
                    {
                        l.is_true = (l.delta == 0) ? 1 : -1;
                    }
                    else
                    {
                        l.is_true = (l.delta <= 0) ? 1 : -1;
                    }
                } // even when a nia lit is not appeared, it can still be determined
            }
            else
            {
                if (l.lits_index != 0 && lit_appear[l.lits_index])
                {
                    l.is_true = (_solution[l.delta.to_int()] > 0) ? 1 : -1;
                } // the boolean lit appears
                else
                {
                    l.is_true = 1;
                } // the boolean lit does not appear, assuming it is true
            }
        }
    }
    // set the sat num of each clause, and sat/unsat a clause
    void ls_solver::initialize_clause_datas()
    {
        _lit_in_unsat_clause_num = 0;
        _bool_lit_in_unsat_clause_num = 0;
        for (uint64_t c = 0; c < _num_clauses; c++)
        {
            clause *cl = &(_clauses[c]);
            cl->sat_count = 0;
            cl->weight = 1;
            for (int l_idx : cl->literals)
            {
                if (l_idx * _lits[std::abs(l_idx)].is_true > 0)
                {
                    cl->sat_count++;
                    cl->watch_lit_idx = l_idx;
                } // determine the sat count and watch lit
            }
            if (cl->sat_count == 0)
            {
                unsat_a_clause(c);
                _lit_in_unsat_clause_num += _clauses[c].literals.size();
                _bool_lit_in_unsat_clause_num += _clauses[c].bool_literals.size();
                for (int l_sign_idx : cl->bool_literals)
                {
                    _vars[_lits[std::abs(l_sign_idx)].delta.to_int()].score++;
                }
            }
            else
            {
                sat_a_clause(c);
            }
            if (cl->sat_count > 0 && cl->sat_count < cl->literals.size())
            {
                sat_clause_with_false_literal->insert_element((int)c);
            }
            // TODO::else{sat_clause_with_false_literal->delete_element((int)c);}
            if (cl->sat_count == 1)
            {
                lit *l = &(_lits[std::abs(cl->watch_lit_idx)]);
                if (!l->is_nia_lit)
                {
                    _vars[l->delta.to_int()].score--;
                }
            }
        }
        total_clause_weight = _num_clauses;
    }

}
