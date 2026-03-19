#pragma once
#include <cstdio>
#include <chrono>
#include <string.h>
#include <stack>
#include <random>
#include <map>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <string>
#include "../utils/nia_Array.h"
#include <algorithm>
#include "../utils/ration_num.h"

namespace nia_overall
{
    const int64_t max_int = __int128_t(INT32_MAX);

    // var_lit means the var appears in corresponding lit, with the coff coffient
    struct var_lit
    {
        uint64_t var_idx;
        uint64_t lit_idx;
        ration_num coff;
        var_lit(uint64_t _var_idx, uint64_t _lit_idx, ration_num _coff) : var_idx(_var_idx), lit_idx(_lit_idx), coff(_coff) {};
    };

    struct coff_var
    {
        int var_idx;
        ration_num coff;
        coff_var() {};
        coff_var(int _var_idx, ration_num _coff) : var_idx(_var_idx), coff(_coff) {};
    };
    // if is_nia_lit: \sum coff*var<=key
    // else:_vars[delta]
    struct lit
    {
        std::vector<coff_var> coff_vars; // sort by var
        std::vector<var_lit> var_lits;   // sort by var
        ration_num key;
        int lits_index;        // lits_index<0 means it is already false, lits_index == 0 means it is already true
        ration_num delta;      // the current value of left side
        bool is_equal = false; // true means a-b-k==0, else a-b-k<=0
        bool is_nia_lit = false;
        int is_true; // 1 means the lit is true, -1 otherwise
    };

    struct variable
    {
        std::vector<int> clause_idxs;
        std::vector<bool> bool_var_in_pos_clause; // true means the boolean var is the pos form in corresponding clause
        std::vector<var_lit> var_lits;            // sort by lit
        std::vector<uint64_t> literal_idxs;
        std::string var_name;
        ration_num low_bound = -max_int;
        ration_num upper_bound = max_int;
        bool is_nia;
        bool is_delete = false;
        int score;       // if it is a bool var, then the score is calculated beforehand
        int up_bool = 0; // the bool value of variables deleted(1 true -1 false)
    };

    struct clause
    {
        std::vector<int> literals; // literals[i]=l means the ith literal of the clause if the pos(neg) of the _lits, it can be negative
        std::vector<int> nia_literals;
        std::vector<int> bool_literals;
        int weight = 1;
        int sat_count;
        int watch_lit_idx; // the lit_idx of sat literal in the clause
        bool is_delete = false;
    };

    // used for restore and record info
    struct tmp_info
    {
        std::vector<variable> _vars_info;
        std::vector<clause> _clauses_info;
        std::vector<lit> _lits_info;
        std::vector<int> fa_info;
        std::vector<ration_num> fa_coff_info;
        std::vector<ration_num> fa_const_info;
        std::vector<ration_num> preset_values_info;
        bool current_build_unsat = true;
    };

    class ls_solver
    {
    public:
        // basic information
        uint64_t _num_vars;
        uint64_t _num_lits;
        int _num_nia_lits;
        int _num_bool_lits;
        uint64_t _num_clauses;
        uint64_t _num_opt = 0; // the number of vars in all literals, which is the max number of operations
        uint64_t unchanged_var_1 = UINT32_MAX;
        uint64_t unchanged_var_2 = UINT32_MAX;
        std::string basic_component_name;
        size_t b_width_idx = 0, b_hight_idx = 0;
        std::vector<variable> _vars;
        std::vector<int> nia_var_vec;
        std::vector<int> bool_var_vec;
        std::vector<lit> _lits;
        std::vector<int> _lit_make_break; // making a move will make or break the lit itself (1:make, -1:break, 0:no change)
        std::vector<clause> _clauses;
        std::vector<clause> _reconstruct_stack;
        std::vector<bool> is_in_unsat_clause;
        std::vector<int> lits_in_unsat_clause;
        Array *unsat_clauses = NULL;
        Array *sat_clause_with_false_literal = NULL; // clauses with 0<sat_num<literal_num, from which swap operation are choosen
        Array *contain_bool_unsat_clauses = NULL;    // unsat clause with at least one boolean var
        Array *false_lit_occur = NULL;               // the false lits for choosing critical move
        uint64_t last_op_var;
        ration_num last_op_value; // the last value and last var, x +1, at least at next step, x -1 is forbidden
        std::map<std::string, int> feasible2litidx;
        // solution
        std::vector<ration_num> _solution;
        std::vector<ration_num> _best_solutin;
        int best_found_cost;
        int best_found_this_restart;
        int no_improve_cnt_bool = 0;
        int no_improve_cnt_nia = 0;
        bool is_in_bool_search = false;
        int _best_found_hard_cost_this_bool;
        int _best_found_hard_cost_this_nia;
        // control
        uint64_t _step;
        uint64_t _outer_layer_step;
        const uint64_t _max_step;
        std::vector<uint64_t> tabulist; // tabulist[2*var] and [2*var+1] means that var are forbidden to move forward or backward until then
        std::vector<bool> is_chosen_bool_var;
        std::vector<uint64_t> last_move;
        std::vector<uint64_t> operation_var_idx_vec;
        std::vector<ration_num> operation_change_value_vec;
        std::vector<int> operation_var_idx_bool_vec;
        std::chrono::steady_clock::time_point start;
        double best_cost_time;
        double _cutoff;
        std::mt19937 mt;
        const uint64_t _additional_len;
        std::map<std::string, uint64_t> name2var; // map the name of a variable to its index
        std::vector<ration_num> _pre_value_1;     // the 1st pre-set value of a var, if the var is in the form of (a==0 OR a==1)
        std::vector<ration_num> _pre_value_2;
        bool use_swap_from_from_small_weight;
        std::vector<bool> var_appear; // true means the var exists in the formula
        std::vector<bool> lit_appear; // true means the lit exists in the formula
        Array *lits_in_cls = NULL;
        const bool _complete_ls;
        void add_unit_clause(std::vector<std::vector<int>> &vec, const std::vector<int> &unit_lits);
        // data structure for clause weighting
        const uint64_t smooth_probability;
        uint64_t _swt_threshold;
        float _swt_p; // w=w*p+ave_w*(1-p)
        uint64_t total_clause_weight;
        int _lit_in_unsat_clause_num;
        int _bool_lit_in_unsat_clause_num;
        // data structure for equality
        // fa[x]=f means x=(coff*f)+const
        std::vector<int> fa;
        std::vector<ration_num> fa_coff;
        std::vector<ration_num> fa_const;
        void prepare_coff_var();

        // input transformation
        bool has_unidentified_lits = false; // if the literals contains unidentified lits
        void read_from_file(const std::string &file_name, const std::vector<std::string> &soft_c_names = {});
        void read_from_string(const std::string &content, const std::vector<std::string> &soft_c_names = {});
        void split_string(std::string &in_string, std::vector<std::string> &str_vec, std::string pattern);
        void build_lits(std::string &in_string);
        void build_instance_original(const std::vector<std::string> &unit_lits = {});
        void build_instance_new_width(int _bc_width, int _bc_hight = 0, int _radius = 0);
        void find_bound(bool &modified);
        void prepare_clause_for_resolution(const std::vector<std::vector<int>> clause_vec);
        void find_unit_equal_lits(std::vector<int> &unit_equal_lits_all);
        void convert_inequal_to_equal(bool &modified);
        void prepare_fa_coffs();
        void prepare_cls_lit_idx_for_vars();
        void preprocess_on_size(ration_num _bc_width = 0, ration_num _bc_hight = 0, ration_num _radius = 0); // find out the equalilty among vars, and reset the vars and lits
        void fix_value(ration_num &_bc_width, ration_num &_bc_hight, ration_num &_radius);
        int bc_width_idx = 0;
        int bc_hight_idx = 0;
        int bc_x_idx=0;
        int bc_y_idx=0;
        void eliminate_multiple_inequalities(bool &modified); // a+b<=3 a+b<=2 -->a+b<=2
        bool resolve_multiple_equals(const std::vector<int> &m_lits);
        void update_by_equals(bool &modified);
        std::vector<ration_num> preset_values;
        void delete_redundant_clauses(std::vector<std::vector<int>> &clause_vec);
        int find(int var_idx); // return the fa of var_idx
        bool merge(lit &l);    // l: a*x + b*y +c == 0
        void update_bound_by_merge(int var_idx_1, int var_idx_2, ration_num coff, ration_num const_term);
        void update_lit_equal(int lit_idx);
        void update_lit_preset_var(int lit_idx);
        uint64_t transfer_name_to_var(const std::string &name, bool is_nia);
        void determine_lit_appear(); // determine whether the lit appear in clauses
        void find_max_var_lit(std::vector <int> & lit_idxs);      // find the max var and lit index in the formula

        // initialize
        ls_solver();
        ls_solver(int random_seed, uint64_t max_step = UINT64_MAX, double cutoff = 1200, bool complete_ls = false);
        void make_space();
        void record_info_after_read_file();
        void restore_infor_before_build_origin();
        std::vector<variable> _vars_after_read_file;
        std::vector<lit> _lits_after_read_file;
        std::vector<std::vector<int>> original_vec;
        void record_info(tmp_info &info);
        void restore_info(const tmp_info &info);
        tmp_info info_before_trial;
        tmp_info info_after_origin;
        void make_lits_space(uint64_t num_lits)
        {
            _num_lits = num_lits;
            _lits.resize(num_lits + _additional_len);
        };
        void initialize();
        void initialize_variable_datas();
        void initialize_lit_datas();
        void initialize_clause_datas();
        void unit_prop();
        void resolution();
        std::vector<int> up_value_vars;
        void prepare_up_value_vars();
        __int128_t hash_lits_to_num(std::vector<int> &lits);
        void reduce_duplicated_lits(bool &modified); // return true if found duplicated lits
        bool lits_been_modified = true;              // whether these lits have modified since last removing duplicated lits
        void gcd_for_lit(lit &l);
        void reduce_clause(bool &modified);
        void determine_lits_by_bound(bool &modified);
        void set_pre_value();
        void read_model();
        bool is_same_lits(std::vector<int> &lits_1, std::vector<int> &lits_2); // deterine whether 2 cls are the same
        bool is_same_lit(const lit &l1, const lit &l2);
        bool is_neg_lit(const lit &l1, const lit &l2);
        bool build_unsat = false;

        // random walk
        void update_clause_weight();
        void smooth_clause_weight();
        void random_walk();
        void no_operation_random_walk(); // when there is no operation, simply find a lit in a random false clause, pick a random var with coff!=0, set it to 0

        // construction
        void construct_solution();
        inline bool is_signed_lit_true(int l_idx) { return (l_idx ^ _lits[std::abs(l_idx)].is_true) >= 0; };

        // basic operations
        inline void sat_a_clause(uint64_t clause_idx)
        {
            unsat_clauses->delete_element((int)clause_idx);
            contain_bool_unsat_clauses->delete_element((int)clause_idx);
        };
        inline void unsat_a_clause(uint64_t clause_idx)
        {
            unsat_clauses->insert_element((int)clause_idx);
            if (_clauses[clause_idx].bool_literals.size() > 0)
                contain_bool_unsat_clauses->insert_element((int)clause_idx);
        };
        bool update_best_solution();
        int pick_critical_move(ration_num &best_value);
        int pick_critical_move_bool();
        void critical_move(uint64_t var_idx, ration_num change_value);
        void move_update(uint64_t var_idx, ration_num change_value);
        void move_update(uint64_t var_idx); // dedicated for boolean var
        void invert_lit(lit &l);
        ration_num delta_lit(lit &l);
        void add_coff(uint64_t var_idx_curr, bool use_tabu, int lit_idx, int &operation_idx, ration_num coff_1);
        double TimeElapsed();
        void clear_prev_data();
        void insert_operation(uint64_t var_idx, ration_num change_value, int &operation_idx, bool use_tabu);
        void add_bool_operation(bool use_tabu, int lit_idx, int &operation_idx_bool);      // given a false lit, add the corresponding var to the operation_vec_bool
        void add_operation_from_false_lit(bool use_tabu, int lit_idx, int &operation_idx); // given a false lit(lit_idx<0 means it is the neg form)
        void select_best_operation_from_vec(int operation_idx, int &best_score, int &best_var_idx, ration_num &best_value);
        void select_best_operation_from_bool_vec(int operation_idx_bool, int &best_score_bool, int &best_var_idx_bool);
        void add_swap_operation(int &operation_idx);
        void swap_from_small_weight_clause();
        void enter_nia_mode();
        void enter_bool_mode();
        bool update_inner_best_solution();
        bool update_outer_best_solution();
        // print
        void print_formula();
        void print_literal(lit &l);
        void print_formula_smt();
        void print_lit_smt(int lit_idx);
        void print_mv();
        void print_full_model();
        void print_components(int offset_x = 0, int offset_y = 0, std::ostream &out = std::cout);
        void print_component(double &x, double &y, double &w, double &h, int &v, int c_idx, int offset_x = 0, int offset_y = 0);
        void print_mv_vars(uint64_t var_idx);
        void print_end();
        void print_var_solution(std::string &var_name, std::string &var_value);
        bool print_var_solution(std::string &var_name, ration_num &var_value);
        void record_soft_var_solution(std::vector<int> &soft_c_info);
        std::vector<size_t> components_idx;
        std::vector<std::string> component_names;
        std::vector<size_t> soft_components_idx;
        void prepare_components_idx();
        void add_component(const std::string &c_name);
        void prepare_soft_components_idx(const std::vector<std::string> &soft_c_names);
        void up_bool_vars();
        bool get_lits_value(uint64_t lit_idx, const std::string &str);
        // calculate score
        int critical_score(uint64_t var_idx, ration_num change_value);
        __int128_t critical_subscore(uint64_t var_idx, ration_num change_value);
        // check
        bool check_solution();
        // handle 128
        inline __int128_t abs_128(__int128_t n) { return n >= 0 ? n : -n; }
        std::string print_128(__int128 n);
        // local search
        bool assume_one_literal(int lit_idx);
        bool assume_one_literal();
        bool local_search();
        // delete
        ~ls_solver();
        // update increment
        bool update_preset_value(uint64_t var_idx, ration_num new_pre_val);
        bool update_width_hight(ration_num new_pre_val_width, ration_num new_pre_val_hight);
    };
}
