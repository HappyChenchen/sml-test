#include "nia_ls.h"
#include "matrix.h"
#include <sstream>
#include <unordered_map>
#define NLS_DEBUG
namespace nia_overall
{
    // input transformation
    void ls_solver::split_string(std::string &in_string, std::vector<std::string> &str_vec, std::string pattern = " ")
    {
        std::string::size_type pos;
        in_string += pattern;
        size_t size = in_string.size();
        for (size_t i = 0; i < size; i++)
        {
            pos = in_string.find(pattern, i);
            if (pos < size)
            {
                std::string s = in_string.substr(i, pos - i);
                str_vec.push_back(s);
                i = pos + pattern.size() - 1;
            }
        }
    }

    bool is_same_cls(const std::vector<int> &cl_1, const std::vector<int> &cl_2)
    {
        if (cl_1.size() != cl_2.size())
        {
            return false;
        }
        else
        {
            for (int l_idx = 0; l_idx < cl_1.size(); l_idx++)
            {
                if (cl_1[l_idx] != cl_2[l_idx])
                {
                    return false;
                }
            }
        }
        return true;
    }

    void ls_solver::read_from_file(const std::string &file_name, const std::vector<std::string> &soft_c_names)
    {
        std::string in_string;
        uint64_t num_lits;
        FILE *fp = freopen(file_name.c_str(), "r", stdin);
        if (fp == NULL)
        {
            perror("fopen failed");
            exit(EXIT_FAILURE);
        }
        std::cin >> num_lits;
        make_lits_space(num_lits);
        getline(std::cin, in_string);
        getline(std::cin, in_string);
        while (in_string != "0")
        {
            build_lits(in_string);
            getline(std::cin, in_string);
        }
        int size;
        std::cin >> size;
        original_vec.resize(size);
        int size_now = 0;
        while (size_now < size)
        {
            std::cin >> in_string;
            if (in_string == "(")
                continue;
            else if (in_string == ")")
                size_now++;
            else
                original_vec[size_now].push_back(atoi(in_string.c_str()));
        }
        delete_redundant_clauses(original_vec);
        basic_component_name = "BC";
        bc_width_idx = (int)transfer_name_to_var(".w", true);
        bc_hight_idx = (int)transfer_name_to_var("BC_hight", true);
        prepare_components_idx();
        prepare_soft_components_idx(soft_c_names);
        _num_vars = _vars.size();
        lit_appear.resize(_num_lits);
        if (lits_in_cls != NULL)
            delete lits_in_cls;
        lits_in_cls = new Array((int)_num_lits);
        record_info_after_read_file();
    }

    void ls_solver::read_from_string(const std::string &content, const std::vector<std::string> &soft_c_names)
    {
        std::istringstream iss(content);
        std::string in_string;
        uint64_t num_lits;
        iss >> num_lits;
        make_lits_space(num_lits);
        std::getline(iss, in_string); // consume rest of first line
        std::getline(iss, in_string); // "0 true"
        while (std::getline(iss, in_string))
        {
            if (in_string == "0") break;
            if (in_string.empty()) continue;
            build_lits(in_string);
        }
        int size;
        iss >> size;
        original_vec.resize(size);
        int size_now = 0;
        while (size_now < size)
        {
            iss >> in_string;
            if (in_string == "(")
                continue;
            else if (in_string == ")")
                size_now++;
            else
                original_vec[size_now].push_back(atoi(in_string.c_str()));
        }
        delete_redundant_clauses(original_vec);
        basic_component_name = "BC";
        bc_width_idx = (int)transfer_name_to_var(".w", true);
        bc_hight_idx = (int)transfer_name_to_var("BC_hight", true);
        prepare_components_idx();
        prepare_soft_components_idx(soft_c_names);
        _num_vars = _vars.size();
        lit_appear.resize(_num_lits);
        if (lits_in_cls != NULL)
            delete lits_in_cls;
        lits_in_cls = new Array((int)_num_lits);
        record_info_after_read_file();
    }

    void ls_solver::record_info_after_read_file()
    {
        _lits_after_read_file = _lits;
        _vars_after_read_file = _vars;
    }

    // restore the vars/lits/clauses
    void ls_solver::restore_infor_before_build_origin()
    {
        for (int idx = 0; idx < _num_lits; idx++)
            lits_in_cls->insert_element(idx);
        std::fill(lit_appear.begin(), lit_appear.end(), true);
        _reconstruct_stack.clear();
        _clauses.clear();
        _lits = _lits_after_read_file;
        _vars = _vars_after_read_file;
    }

    void ls_solver::add_unit_clause(std::vector<std::vector<int>> &vec, const std::vector<int> &unit_lits)
    {
        size_t ori_vec_num = vec.size();
        vec.resize(ori_vec_num + unit_lits.size());
        for (size_t idx = 0; idx < unit_lits.size(); idx++)
        {
            vec[ori_vec_num + idx].emplace_back(unit_lits[idx]);
        }
    }

    void ls_solver::build_lits(std::string &in_string)
    {
        std::vector<std::string> vec;
        split_string(in_string, vec);
        if (vec[0] == "0")
        {
            _lits[0].lits_index = 0;
            return;
        } // true literal
        int lit_index = std::atoi(vec[0].c_str());
        lit *l = &(_lits[lit_index]);
        if (vec[1] == "or" || vec[1] == "if")
        {
            l->delta = (int)transfer_name_to_var(vec[2], false);
            l->key = 1;
            l->is_nia_lit = false;
            l->lits_index = lit_index;
            return;
        } // or lit in the form: 1 or newvar_2
        if (vec.size() > 2)
        {
            l->is_nia_lit = true;
            if (vec.size() > 6)
            {
                l->lits_index = std::atoi(vec[0].c_str());
                int idx = 5;
                if (vec[2] == "=" && vec[3] != "(")
                {
                    idx++;
                    l->key = -std::atoll(vec[3].c_str());
                }
                l->is_equal = (vec[2] == "=");
                for (; idx < vec.size(); idx++)
                {
                    if (vec[idx] == ")")
                    {
                        break;
                    }
                    if (vec[idx] == "(")
                    {
                        idx += 2;
                        int coff = std::atoi(vec[idx].c_str());
                        uint64_t v_idx = transfer_name_to_var(vec[++idx], true); //( * 115 x ) idx at x
                        l->coff_vars.push_back(coff_var((int)v_idx, coff));
                        idx++;
                    }
                    else
                    {
                        uint64_t v_idx = transfer_name_to_var(vec[idx], true);
                        l->coff_vars.push_back(coff_var((int)v_idx, 1));
                    }
                    _num_opt += l->coff_vars.size();
                }
                if (vec[2] != "=" || vec[3] == "(")
                {
                    l->key = -std::atoi(vec[++idx].c_str());
                }
                if (vec[2] == ">=")
                {
                    l->key += ration_num(1);
                    invert_lit(*l);
                }
            } //( <= ( + x1 ( * -1 x2 ) x7 ( * -1 x8 ) ) 0 )
            else
            {
                l->lits_index = std::atoi(vec[0].c_str());
                if (vec[2] == "=" && vec.size() == 6 && (vec[3][0] < '0' || vec[3][0] > '9') && (vec[4][0] < '0' || vec[4][0] > '9') && (vec[3][0] != '-') && (vec[4][0] != '-'))
                {
                    l->is_equal = true;
                    l->key = 0;
                    uint64_t new_v_idx_1 = transfer_name_to_var(vec[3], true);
                    uint64_t new_v_idx_2 = transfer_name_to_var(vec[4], true);
                    l->coff_vars.push_back(coff_var((int)new_v_idx_1, 1));
                    l->coff_vars.push_back(coff_var((int)new_v_idx_2, -1));
                } //( = x1 x2 )
                else
                {
                    int bound;
                    uint64_t var_idx;
                    if ((vec[3][0] < '0' || vec[3][0] > '9') && (vec[3][0] != '-'))
                    {
                        bound = std::atoi(vec[4].c_str());
                        var_idx = transfer_name_to_var(vec[3], true);
                    } //( >= x 0 )
                    else
                    {
                        bound = std::atoi(vec[3].c_str());
                        var_idx = transfer_name_to_var(vec[4], true);
                    } //( = 0 x )
                    if (vec[2] == ">=")
                    {
                        l->key = bound;
                        l->coff_vars.push_back(coff_var((int)var_idx, -1));
                    }
                    else
                    {
                        l->key = -bound;
                        l->coff_vars.push_back(coff_var((int)var_idx, 1));
                    }
                    l->is_equal = (vec[2] == "=");
                } //( >= x 0 )
            }

        } // nia lit
        else
        {
            l->delta = (int)transfer_name_to_var(vec[1], false);
            if (vec[1].find("feasible") != std::string::npos || vec[1].find("soft_var") != std::string::npos)
                feasible2litidx[vec[1]] = lit_index;
            l->key = 1;
            l->is_nia_lit = false;
            l->lits_index = lit_index;
        } // boolean lit
    }

    int ls_solver::find(int var_idx)
    {
        if (var_idx == fa[var_idx])
        {
            fa_coff[var_idx] = 1;
            fa_const[var_idx] = 0;
            return var_idx;
        }
        else
        {
            int old_fa = fa[var_idx]; // var = old_fa * fa_coff[var] + fa_const[var] -> (new_fa*fa_coff[old_fa]+fa_const[old_fa]) * fa_coff[var] + fa_const[var]
            int new_fa = find(fa[var_idx]);
            fa_const[var_idx] += fa_coff[var_idx] * fa_const[old_fa];
            fa_coff[var_idx] *= fa_coff[old_fa];
            return fa[var_idx] = new_fa;
        }
    }

    // var_1 = coff*var_2 + const_term
    // 基于该等式，和var_2的bound，更新var_1的bound
    void ls_solver::update_bound_by_merge(int var_idx_1, int var_idx_2, ration_num coff, ration_num const_term)
    {
        variable &var_1 = _vars[var_idx_1];
        variable &var_2 = _vars[var_idx_2];
        if (var_2.upper_bound != max_int)
        {
            if (coff > 0 && var_1.upper_bound > coff * var_2.upper_bound + const_term)
                var_1.upper_bound = coff * var_2.upper_bound + const_term;
            else if (coff < 0 && var_1.low_bound < coff * var_2.upper_bound + const_term)
                var_1.low_bound = coff * var_2.upper_bound + const_term;
        }
        if (var_2.low_bound != -max_int)
        {
            if (coff > 0 && var_1.low_bound < coff * var_2.low_bound + const_term)
                var_1.low_bound = coff * var_2.low_bound + const_term;
            else if (coff < 0 && var_1.upper_bound > coff * var_2.low_bound + const_term)
                var_1.upper_bound = coff * var_2.low_bound + const_term;
        }
    }

    // return true if the fa_coff and fa_const have been modified
    // 此处要把bound给传递过去
    bool ls_solver::merge(lit &l)
    { // coff_1*var_1==coff_2*var_2
        int var_idx_1 = l.coff_vars[0].var_idx;
        int var_idx_2 = l.coff_vars[1].var_idx;
        int fa_1 = find(var_idx_1), fa_2 = find(var_idx_2);
        ration_num const_1 = l.coff_vars[0].coff * fa_const[var_idx_1], const_2 = l.coff_vars[1].coff * fa_const[var_idx_2];
        ration_num fa_coff_1 = l.coff_vars[0].coff * fa_coff[var_idx_1], fa_coff_2 = l.coff_vars[1].coff * fa_coff[var_idx_2];
        ration_num key_new = l.key + const_1 + const_2;
        // fa_coff_1*fa_1+fa_coff_2*fa_2+key==0
        if (fa_1 == fa_2)
            return false;
        if (fa_coff_1.abs() > fa_coff_2.abs())
        {
            fa[fa_2] = fa_1;
            fa_coff[fa_2] = -fa_coff_1 / fa_coff_2;
            fa_const[fa_2] = -key_new / fa_coff_2;
            update_bound_by_merge(fa_1, fa_2, -fa_coff_2 / fa_coff_1, -key_new / fa_coff_1); // 保留下来的是fa_1,fa_2被从公式中删除了,因此要用fa_2的bound更新fa_1的bound
            return true;
        } // fa_2 = (-fa_coff_1/fa_coff_2)*fa_1 + (-key/fa_coff_2)
        else
        {
            fa[fa_1] = fa_2;
            fa_coff[fa_1] = -fa_coff_2 / fa_coff_1;
            fa_const[fa_1] = -key_new / fa_coff_1;
            update_bound_by_merge(fa_2, fa_1, -fa_coff_1 / fa_coff_2, -key_new / fa_coff_2);
            return true;
        } // fa_1 = (-fa_coff_2/fa_coff_1)*fa_2 + (-key/fa_coff_1)
        return false;
    }
    bool cmp_coff_var(const coff_var &cv1, const coff_var &cv2) { return cv1.var_idx < cv2.var_idx; }

    // if a+b+...<=k and a+b+...>=k only exist in 2 clauses respectively, and these 2 clause have common literals, then convert them to a+b+...=k
    void ls_solver::convert_inequal_to_equal(bool &modified)
    {
        // 首先遍历所有子句，找到非等式文字所在的子句号
        std::vector<std::vector<int>> inequal_lit_in_clauses; // 记录每个非等式文字所在的子句号
        inequal_lit_in_clauses.resize(_num_lits);
        for (unsigned clause_idx = 0; clause_idx < _clauses.size(); clause_idx++)
            for (int lit_idx : _clauses[clause_idx].literals)
                if (lit_idx > 0 && _lits[lit_idx].is_nia_lit && !_lits[lit_idx].is_equal && _lits[lit_idx].lits_index != 0)
                    inequal_lit_in_clauses[lit_idx].push_back(clause_idx);

        std::vector<int> inequals; // 记录所有只出现在一个子句中的非等式文字
        for (size_t l_idx = 0; l_idx < _num_lits; l_idx++)
            if (inequal_lit_in_clauses[l_idx].size() == 1)
                inequals.push_back(l_idx);
        // 遍历所有非等式文字，找到所有 a+b+...<=k 和 a+b+...>=k pairs，判断其是否只出现在一个clause中（即子句号数为1），并且判断其包含它们的子句除了该文字外一样，如果一样就将a+b+...<=k转为a+b+...=k，并删除a+b+...>=k
        for (size_t idx_1 = 0; idx_1 < inequals.size(); idx_1++)
        {
            int pre_idx = inequals[idx_1];
            if (_lits[pre_idx].lits_index == 0)
                continue;
            for (size_t idx_2 = idx_1 + 1; idx_2 < inequals.size(); idx_2++)
            {

                int pos_idx = inequals[idx_2];
                if (is_neg_lit(_lits[pre_idx], _lits[pos_idx]))
                {
                    // check whether the clauses are the same except these two lits
                    std::vector<int> &clause_pre_lits = _clauses[inequal_lit_in_clauses[pre_idx][0]].literals;
                    std::vector<int> &clause_pos_lits = _clauses[inequal_lit_in_clauses[pos_idx][0]].literals;
                    if (clause_pre_lits.size() != clause_pos_lits.size())
                        continue;
                    std::unordered_map<int, int> count;
                    for (int x : clause_pre_lits)
                        ++count[x];
                    for (int x : clause_pos_lits)
                        --count[x];
                    count[pre_idx]--; // 移除 pre_idx 的影响
                    count[pos_idx]++; // 移除 pos_idx 的影响
                    bool is_same = true;
                    for (auto [_, v] : count)
                        if (v != 0)
                        {
                            is_same = false;
                            break;
                        }
                    if (is_same)
                    {
                        modified = true;
                        _lits[pre_idx].is_equal = true;
                        _lits[pos_idx].lits_index = 0;
                        break;
                    }
                }
            }
        }
    }

    void ls_solver::find_unit_equal_lits(std::vector<int> &unit_equal_lits_all)
    {
        // find out all unit equal lits
        for (int clause_idx = 0; clause_idx < _clauses.size(); clause_idx++)
        {
            if (_clauses[clause_idx].literals.size() == 1 && _clauses[clause_idx].literals[0] > 0)
            {
                int lit_idx = _clauses[clause_idx].literals[0];
                if (_lits[lit_idx].is_equal)
                { // t1+t2+..+tn=0
                    unit_equal_lits_all.push_back(lit_idx);
                }
            }
        }
    }

    void ls_solver::fix_value(ration_num &_bc_width, ration_num &_bc_hight, ration_num &_radius)
    {
        if (_bc_width != 0)
        {
            int root_bc_width_idx = find(bc_width_idx);
            _bc_width = (_bc_width - fa_const[bc_width_idx]) / fa_coff[bc_width_idx];
            preset_values[root_bc_width_idx] = _bc_width;
            _vars[root_bc_width_idx].upper_bound = _bc_width;
            _vars[root_bc_width_idx].low_bound = _bc_width;
        }
        if (_bc_hight != 0)
        {
            int root_bc_hight_idx = find(bc_hight_idx);
            _bc_hight = (_bc_hight - fa_const[bc_hight_idx]) / fa_coff[bc_hight_idx];
            preset_values[root_bc_hight_idx] = _bc_hight;
            _vars[root_bc_hight_idx].upper_bound = _bc_hight;
            _vars[root_bc_hight_idx].low_bound = _bc_hight;
        }
        if (_radius != 0)
        {
            int radius_idx = (int)transfer_name_to_var("radius", true);
            int root_radius_idx = find(radius_idx);
            _radius = (_radius - fa_const[radius_idx]) / fa_coff[radius_idx];
            preset_values[root_radius_idx] = _radius;
            _vars[root_radius_idx].upper_bound = _radius;
            _vars[root_radius_idx].low_bound = _radius;
        }
    }

    void ls_solver::preprocess_on_size(ration_num _bc_width, ration_num _bc_hight, ration_num _radius)
    {
        fix_value(_bc_width, _bc_hight, _radius);
        bool modified = true;
        while (modified && !build_unsat)
        { // if the formula has been modified, it should try to update by equalities again
            modified = false;
            reduce_clause(modified);
            convert_inequal_to_equal(modified); // 将不等式转为等式
            update_by_equals(modified);         // 通过等式来更新文字
            reduce_clause(modified);
            find_bound(modified); // 找到变量的上下界
            // reduce_clause(modified);
            eliminate_multiple_inequalities(modified); // 删除多余的不等式
        }
        reduce_clause(modified);
    }
    // initialize the fa, fa_coff, fa_const, preset_values
    void ls_solver::prepare_fa_coffs()
    {
        fa.resize(_vars.size());
        for (int var_idx = 0; var_idx < _vars.size(); var_idx++)
            fa[var_idx] = var_idx;
        fa_coff.resize(_vars.size());
        fa_const.resize(_vars.size());
        preset_values.resize(_vars.size());
        std::fill(fa_coff.begin(), fa_coff.end(), 1);
        std::fill(fa_const.begin(), fa_const.end(), 0);
        std::fill(preset_values.begin(), preset_values.end(), INT32_MAX);
    }

    // return true if lits can be modified by equalities
    // 通过等式来更新文字: 会尝试找a==k 或者 a+M*b==k的等式来更新文字，如果文字直接为真或假就可以直接删除了
    void ls_solver::update_by_equals(bool &modified_in_this_call)
    {
        std::vector<int> unit_equal_lits_all; // a+b+... == any
        find_unit_equal_lits(unit_equal_lits_all);
        bool find_new_merge = true;
        bool modified = false;
        while (find_new_merge)
        {
            find_new_merge = false;
            // find the equalities in form of a==1
            for (int i = 0; i < unit_equal_lits_all.size(); i++)
            {
                lit *l = &(_lits[unit_equal_lits_all[i]]);
                if (l->lits_index != 0 && l->coff_vars.size() == 1)
                {
                    int var_1 = l->coff_vars[0].var_idx;
                    if (preset_values[var_1] == INT32_MAX)
                    {
                        preset_values[var_1] = ((-l->key) / l->coff_vars[0].coff);
                        _vars[var_1].upper_bound = preset_values[var_1];
                        _vars[var_1].low_bound = preset_values[var_1];
                    }
                    find_new_merge = true;
                    modified = true;
                    l->lits_index = 0;
                }
            }
            for (int i = 0; i < unit_equal_lits_all.size(); i++)
            {
                int l_idx = unit_equal_lits_all[i];
                update_lit_preset_var(l_idx);
            }
            // merge the equalities in form of 1*a+2*b==3
            for (int i = 0; i < unit_equal_lits_all.size(); i++)
            {
                lit *l = &(_lits[unit_equal_lits_all[i]]);
                if (l->lits_index <= 0)
                {
                    continue;
                }
                if (l->coff_vars.size() == 2 && merge(*l))
                {
                    l->lits_index = 0;
                    find_new_merge = true;
                    modified = true;
                }
            }
            // modify the lit by new equality
            for (int i = 0; i < unit_equal_lits_all.size(); i++)
                update_lit_equal(unit_equal_lits_all[i]);

            // if there is no unit equalities with 1 or 2 vars, try to find one by modify the formula
            if (!modified)
            {
                std::vector<int> m_lits;
                for (const auto &l_idx : unit_equal_lits_all)
                {
                    if (_lits[l_idx].coff_vars.size() > 2)
                        m_lits.push_back(l_idx);
                }
                if (resolve_multiple_equals(m_lits))
                {
                    modified = true; // if the equality has been modified, and find some literals with 1 or 2 vars
                }
            }
        }
        for (int idx = 0; idx < lits_in_cls->size(); idx++)
        {
            int lit_idx = lits_in_cls->element_at(idx);
            update_lit_preset_var(lit_idx);
            update_lit_equal(lit_idx);
        }
        if (modified)
            modified_in_this_call = true;
    }

    void ls_solver::update_lit_preset_var(int lit_idx)
    {
        lit *l = &(_lits[lit_idx]);
        if (l->lits_index <= 0 || !l->is_nia_lit)
            return;
        if (l->coff_vars.size() == 1 && l->is_equal)
            return;
        unsigned total_cv_size = 0;
        for (unsigned cv_idx = 0; cv_idx < l->coff_vars.size(); cv_idx++)
        {
            coff_var &cv = l->coff_vars[cv_idx];
            int var_idx = cv.var_idx;
            if (preset_values[var_idx] != INT32_MAX)
            {
                lits_been_modified = true;
                l->key += (cv.coff * preset_values[var_idx]);
                continue;
            }
            l->coff_vars[total_cv_size++] = cv;
        }
        l->coff_vars.resize(total_cv_size);
        if (l->coff_vars.size() == 0)
        {
            if (l->key == 0 || (l->key < 0 && !l->is_equal))
                l->lits_index = 0;
            else
            {
                l->lits_index = -1;
                l->is_true = false;
            }
        }
        gcd_for_lit(*l);
    }

    void ls_solver::update_lit_equal(int lit_idx)
    {
        lit *l = &(_lits[lit_idx]);
        if (l->lits_index <= 0 || !l->is_nia_lit)
        {
            return;
        }
        bool lit_modified = false;
        for (coff_var &cv : l->coff_vars)
        {
            int var_idx = cv.var_idx;
            int new_var_idx = find(var_idx);
            if (new_var_idx != var_idx)
            { // modify the var
                lit_modified = true;
                l->key += cv.coff * fa_const[var_idx];
                cv.var_idx = new_var_idx;
                cv.coff *= fa_coff[var_idx];
            }
        }
        if (lit_modified)
        {
            lits_been_modified = true;
            std::sort(l->coff_vars.begin(), l->coff_vars.end(), cmp_coff_var);
            int curr_var_idx = l->coff_vars[0].var_idx;
            ration_num curr_coff = 0;
            int lit_coff_var_idx = 0;
            for (int cv_idx = 0; cv_idx < l->coff_vars.size(); cv_idx++)
            {
                if (l->coff_vars[cv_idx].var_idx != curr_var_idx)
                {
                    curr_var_idx = l->coff_vars[cv_idx].var_idx;
                    curr_coff = 0;
                } // enter a new var
                curr_coff += l->coff_vars[cv_idx].coff; // the same var
                if (curr_coff != 0 && (cv_idx == l->coff_vars.size() - 1 || l->coff_vars[cv_idx + 1].var_idx != curr_var_idx))
                {
                    l->coff_vars[lit_coff_var_idx].var_idx = curr_var_idx;
                    l->coff_vars[lit_coff_var_idx++].coff = curr_coff;
                } // the last coff_var of the current var
            }
            l->coff_vars.resize(lit_coff_var_idx);
            if (lit_coff_var_idx == 0)
            {
                if (l->key == 0 || (l->key < 0 && !l->is_equal))
                    l->lits_index = 0;
                else
                {
                    l->lits_index = -1;
                    l->is_true = false;
                }
            }
            gcd_for_lit(*l);
        }
    }

    void print_vec(const std::vector<std::vector<int>> &cl)
    {
        std::cout << "0\n"
                  << cl.size() << '\n';
        for (auto c : cl)
        {
            std::cout << "(";
            for (auto l : c)
            {
                std::cout << " " << l;
            }
            std::cout << " )\n";
        }
    }

    // delete the duplicate clauses and clause containing unit lits
    // 删除完全一样的子句（删除子句中重复的文字），以及包含单元文字的子句（因为以及包含单元文字，则该子句必定为满足）
    void ls_solver::delete_redundant_clauses(std::vector<std::vector<int>> &clause_vec)
    {
#ifdef LS_DEBUG
        std::cout << "start build\n";
        print_vec(clause_vec);
#endif
        std::sort(clause_vec.begin(), clause_vec.end()); // sort the clauses according to its literals
        int n_c = 0;
        for (int cl_idx = 0; cl_idx < clause_vec.size(); cl_idx++)
        {
            if (n_c == 0 || (!is_same_cls(clause_vec[cl_idx], clause_vec[n_c - 1])))
            {
                clause_vec[n_c++] = clause_vec[cl_idx];
            }
        } // delete redundant clauses
        clause_vec.resize(n_c);
        for (int cl_idx = 0; cl_idx < clause_vec.size(); cl_idx++)
        {
            unsigned clause_sz = 0;
            for (int l : clause_vec[cl_idx])
            {
                if (clause_sz == 0 || clause_vec[cl_idx][clause_sz - 1] != l)
                {
                    clause_vec[cl_idx][clause_sz++] = l;
                }
            }
            clause_vec[cl_idx].resize(clause_sz);
        }
#ifdef LS_DEBUG
        std::cout << "after delete redundant clauses\n";
        print_vec(clause_vec);
#endif
        std::vector<bool> unit_lit(2 * _num_lits + _additional_len, false);
        n_c = 0;
        for (int cl_idx = 0; cl_idx < clause_vec.size(); cl_idx++)
        {
            if (clause_vec[cl_idx].size() == 1)
            {
                unit_lit[(clause_vec[cl_idx][0] + _num_lits)] = true;
            } // unit lit, record it
        }
        for (int cl_idx = 0; cl_idx < clause_vec.size(); cl_idx++)
        {
            bool has_unit_lit = false;
            if (clause_vec[cl_idx].size() != 1)
            {
                for (auto l : clause_vec[cl_idx])
                {
                    if (unit_lit[l + _num_lits])
                    {
                        has_unit_lit = true;
                        break;
                    } // if found unit clause
                }
            }
            if (!has_unit_lit && clause_vec[cl_idx].size() > 0)
            {
                clause_vec[n_c++] = clause_vec[cl_idx];
            } // unit or multiple clauses without unit, add it to clauses
        }
        clause_vec.resize(n_c);
#ifdef LS_DEBUG
        std::cout << "after delete unit clauses\n";
        print_vec(clause_vec);
        std::cout.flush();
#endif
    }

    void ls_solver::build_instance_original(const std::vector<std::string> &unit_lits)
    {
        restore_infor_before_build_origin();
        std::vector<std::vector<int>> clause_vec = original_vec;
        std::vector<int> unit_lits_int;
        for (const auto &l : unit_lits)
        {
            unit_lits_int.emplace_back(feasible2litidx[l]);
        }
        add_unit_clause(clause_vec, unit_lits_int);
        delete_redundant_clauses(clause_vec);
        prepare_clause_for_resolution(clause_vec);
        // now the vars are all in the resolution vars
        unit_prop();
        resolution();
        unit_prop();
        bool modified = false;
        reduce_clause(modified);
        prepare_fa_coffs();
        preprocess_on_size();
        record_info(info_after_origin);
        make_space();
    }

    // record the lit/var/clause information after build_origin
    void ls_solver::record_info(tmp_info &info)
    {
        info._clauses_info = _clauses;
        info._lits_info = _lits;
        info._vars_info = _vars;
        info.fa_info = fa;
        info.fa_coff_info = fa_coff;
        info.fa_const_info = fa_const;
        info.preset_values_info = preset_values;
        info.current_build_unsat = build_unsat;
    }

    void ls_solver::restore_info(const tmp_info &info)
    {
        _clauses = info._clauses_info;
        _lits = info._lits_info;
        _vars = info._vars_info;
        fa = info.fa_info;
        fa_coff = info.fa_coff_info;
        fa_const = info.fa_const_info;
        preset_values = info.preset_values_info;
        _num_clauses = _clauses.size();
        build_unsat = info.current_build_unsat;
    }

    void ls_solver::build_instance_new_width(int _bc_width, int _bc_hight, int _radius)
    {
        restore_info(info_after_origin);
        preprocess_on_size(_bc_width, _bc_hight, _radius);
    }

    // 找到出现最多的变量
    // 找到这个变量所在的形如x=100的文字
    void ls_solver::find_max_var_lit(std::vector<int> &lit_idxs)
    {
        // 找到出现最多的变量
        std::vector<int> var_count(_num_vars, 0);
        for (const auto &cls : _clauses)
        {
            for (const auto &lit_idx : cls.literals)
            {
                int abs_lit_idx = std::abs(lit_idx);
                lit &l = _lits[abs_lit_idx];
                if (l.is_nia_lit)
                {
                    for (const auto &cv : l.coff_vars)
                    {
                        var_count[cv.var_idx]++;
                    }
                }
            }
        }
        int max_count = -1;
        int max_var_idx = -1;
        for (int v_idx = 0; v_idx < _num_vars; v_idx++)
        {
            if (var_count[v_idx] > max_count)
            {
                max_count = var_count[v_idx];
                max_var_idx = v_idx;
            }
        }
        // 找到这个变量所在的形如x=100的文字,从lits_in_cls中找
        for (int idx = 0; idx < lits_in_cls->size(); idx++)
        {
            int l_idx = lits_in_cls->element_at(idx);
            lit &l = _lits[l_idx];
            if (l.coff_vars.size() == 1 && l.coff_vars[0].var_idx == max_var_idx)
            {
#ifdef LS_DEBUG
                print_literal(l);
#endif
                lit_idxs.push_back(l_idx);
            }
        }
    }

    bool ls_solver::assume_one_literal()
    {
        std::vector<int> lit_idxs;
        find_max_var_lit(lit_idxs);
        for (const auto &lit_idx : lit_idxs)
        {
            if (assume_one_literal(lit_idx)||(!_lits[lit_idx].is_equal && assume_one_literal(-lit_idx)))
                return true;
        }
        return false;
    }

    // 尝试一个文字假设它为真
    bool ls_solver::assume_one_literal(int lit_idx)
    {
        // 记录下原来的情况
        // 假设这个文字为真
        // 基于这个文字进行化简，并且局部搜索
        // 如果局部搜索不成功再恢复
        if (_clauses.size() == 0)
            return local_search();
        record_info(info_before_trial);
        clause new_cls;
        new_cls.literals.push_back(lit_idx);
        _clauses.push_back(new_cls);
        _num_clauses++;
        preprocess_on_size();
#ifdef LS_DEBUG
        print_formula();
#endif
        if (local_search())
            return true;
        else
        {
            restore_info(info_before_trial);
            return false;
        }
    }

    void ls_solver::prepare_up_value_vars()
    {
        up_value_vars.resize(_num_vars);
        std::fill(up_value_vars.begin(), up_value_vars.end(), 0);
        for (size_t v_idx = 0; v_idx < _num_vars; v_idx++)
        {
            if (_vars[v_idx].up_bool != 0)
                up_value_vars[v_idx] = _vars[v_idx].up_bool;
        }
    }

    // a+b<=10 and a+b<=0 --> a+b<=0
    // 删除多余的等式
    void ls_solver::eliminate_multiple_inequalities(bool &modified)
    {
        std::vector<int> unit_ineqs;
        for (const auto &c : _clauses)
        {
            if (c.literals.size() == 1 && c.literals[0] > 0)
            {
                lit &l = _lits[c.literals[0]];
                if (l.lits_index != 0 && l.is_nia_lit && !l.is_equal)
                    unit_ineqs.emplace_back(l.lits_index);
            }
        }
        size_t unit_ineqs_num = unit_ineqs.size();
        for (size_t idx_pre = 0; idx_pre < unit_ineqs_num; idx_pre++)
        {
            lit *l1 = &_lits[unit_ineqs[idx_pre]];
            if (l1->lits_index == 0)
                continue;
            for (size_t idx_pos = idx_pre + 1; idx_pos < unit_ineqs_num; idx_pos++)
            {
                lit *l2 = &_lits[unit_ineqs[idx_pos]];
                if (l2->lits_index == 0)
                    continue;
                if (l1->coff_vars.size() != l2->coff_vars.size())
                    continue;
                bool is_same = true;
                for (size_t i = 0; i < l1->coff_vars.size(); i++)
                {
                    if ((l1->coff_vars[i].coff != l2->coff_vars[i].coff) || (l1->coff_vars[i].var_idx != l2->coff_vars[i].var_idx))
                    {
                        is_same = false;
                        break;
                    }
                }
                if (is_same)
                {
                    if (l1->key > l2->key)
                    {
                        l2->lits_index = 0;
                        modified = true;
                    }
                    else
                    {
                        l1->lits_index = 0;
                        modified = true;
                        break;
                    }
                }
            }
        }
    }
    // 找到所有单元子句，并根据单元子句更新变量的上下界，会去除原来的单元子句
    void ls_solver::find_bound(bool &modified)
    {
        for (int clause_idx = 0; clause_idx < _clauses.size(); clause_idx++)
        {
            if (_clauses[clause_idx].literals.size() == 1)
            {
                int &l_only_idx = _clauses[clause_idx].literals[0];
                lit *l = &(_lits[std::abs(l_only_idx)]);
                if (l->is_equal || !l->is_nia_lit || l->lits_index <= 0)
                {
                    continue;
                } // equal lit is not bound lit
                if (l->coff_vars.size() == 1)
                {
                    ration_num new_low_bound = -max_int;
                    ration_num new_upper_bound = max_int;
                    int var_idx = l->coff_vars[0].var_idx;
                    if (l_only_idx > 0)
                    {
                        if (l->coff_vars[0].coff > 0)
                        {
                            new_upper_bound = (-l->key) / (l->coff_vars[0].coff);
                        } // ax+k<=0   x<=(-k/a)
                        else
                        {
                            new_low_bound = (-l->key) / (l->coff_vars[0].coff);
                        } // ax+k<=0  x>=(-k/a)
                        l->lits_index = 0;
                        l->is_true = true;
                    }
                    else
                    {
                        if (l->coff_vars[0].coff > 0)
                        {
                            new_low_bound = (-l->key) / (l->coff_vars[0].coff);
                        } // ax+k>0 ax+k>=1 x>=(1-k)/a
                        else
                        {
                            new_upper_bound = (-l->key) / (l->coff_vars[0].coff);
                        } // ax+k>=1 x<=(1-k)/a
                        l->lits_index = -1;
                        l->is_true = false;
                    }
                    if (new_low_bound > _vars[var_idx].low_bound)
                    {
                        _vars[var_idx].low_bound = new_low_bound;
                    }
                    else if (new_upper_bound < _vars[var_idx].upper_bound)
                    {
                        _vars[var_idx].upper_bound = new_upper_bound;
                    }
                    modified = true;
                }
            }
        }
    }
    void ls_solver::prepare_clause_for_resolution(const std::vector<std::vector<int>> clause_vec)
    {
        _clauses.resize(clause_vec.size());
        _num_clauses = 0;
        for (auto clause_curr : clause_vec)
        {
            bool is_tautology = false;
            for (auto l_idx : clause_curr)
            {
                if ((_lits[std::abs(l_idx)].lits_index == 0 && l_idx > 0) || (_lits[std::abs(l_idx)].lits_index < 0 && l_idx < 0))
                {
                    is_tautology = true;
                    break;
                }
            }
            if (is_tautology)
            {
                continue;
            }
            for (auto l_idx : clause_curr)
            {
                _clauses[_num_clauses].literals.push_back(l_idx);
                lit *l = &(_lits[std::abs(l_idx)]);
                if (l->lits_index <= 0)
                {
                    continue;
                }
                if (!l->is_nia_lit)
                {
                    _vars[l->delta.to_int()].clause_idxs.push_back((int)_num_clauses);
                }
            }
            _num_clauses++;
        }
        _clauses.resize(_num_clauses);
    }

    uint64_t ls_solver::transfer_name_to_var(const std::string &name, bool is_nia)
    {
        if (name2var.find(name) == name2var.end())
        {
            name2var[name] = _vars.size();
            variable var;
            var.var_name = name;
            var.is_nia = is_nia;
            _vars.push_back(var);
            if (is_nia)
            {
                nia_var_vec.push_back((int)_vars.size() - 1);
            }
            else
            {
                bool_var_vec.push_back((int)_vars.size() - 1);
            }
            return _vars.size() - 1;
        }
        else
            return name2var[name];
    }

    void ls_solver::unit_prop()
    {
        std::stack<uint64_t> unit_clause; // the var_idx in the unit clause
        for (uint64_t clause_idx = 0; clause_idx < _num_clauses; clause_idx++)
        { // the unit clause is the undeleted clause containing only one bool var
            if (!_clauses[clause_idx].is_delete && _clauses[clause_idx].literals.size() == 1 && !_lits[std::abs(_clauses[clause_idx].literals[0])].is_nia_lit)
            {
                unit_clause.push(clause_idx);
            }
        }
        while (!unit_clause.empty())
        {
            uint64_t unit_clause_idx = unit_clause.top();
            unit_clause.pop();
            if (_clauses[unit_clause_idx].is_delete)
            {
                continue;
            }
            int is_pos_lit = (_clauses[unit_clause_idx].literals[0] > 0) ? 1 : -1;                     // determine the sign of the var in unit clause
            uint64_t unit_var = _lits[std::abs(_clauses[unit_clause_idx].literals[0])].delta.to_int(); // determing the var in unit clause
            _vars[unit_var].is_delete = true;                                                          // delete the unit var
            _vars[unit_var].up_bool = is_pos_lit;                                                      // set the solution of a boolean var as its unit propogation value
            for (uint64_t cl_idx : _vars[unit_var].clause_idxs)
            {
                clause *cl = &(_clauses[cl_idx]);
                if (cl->is_delete)
                    continue;
                for (uint64_t lit_idx = 0; lit_idx < cl->literals.size(); lit_idx++)
                {
                    int l_id_in_lits = cl->literals[lit_idx];
                    lit *l = &(_lits[std::abs(l_id_in_lits)]);
                    if (!l->is_nia_lit && l->delta == unit_var)
                    { // go through the clauses of the unit var, find the var in corresponding clause
                        if ((is_pos_lit > 0 && l_id_in_lits > 0) || (is_pos_lit < 0 && l_id_in_lits < 0))
                        {
                            cl->is_delete = true;
                            for (int l_idx_inner : cl->literals)
                            { // delete the clause from corresponding bool var
                                lit *l_inner = &(_lits[std::abs(l_idx_inner)]);
                                if (!l_inner->is_nia_lit && l_inner->delta != unit_var)
                                {
                                    variable *var_inner = &(_vars[l_inner->delta.to_int()]);
                                    for (uint64_t delete_idx = 0; delete_idx < var_inner->clause_idxs.size(); delete_idx++)
                                    {
                                        if (var_inner->clause_idxs[delete_idx] == cl_idx)
                                        {
                                            var_inner->clause_idxs[delete_idx] = var_inner->clause_idxs.back();
                                            var_inner->clause_idxs.pop_back();
                                            break;
                                        }
                                    }
                                }
                            }
                        } // if there exist same lit, the clause is already set true, then delete the clause
                        else
                        { // else delete the lit
                            cl->literals[lit_idx] = cl->literals.back();
                            cl->literals.pop_back();
                            if (cl->literals.size() == 1 && !_lits[std::abs(cl->literals[0])].is_nia_lit)
                            {
                                unit_clause.push(cl_idx);
                            } // if after deleting, it becomes unit clause
                        }
                        break;
                    }
                }
            }
        }
    }
    __int128_t ls_solver::hash_lits_to_num(std::vector<int> &lits)
    {
        std::sort(lits.begin(), lits.end());
        __int128_t hash_num = 0;
        for (int lit_idx : lits)
        {
            hash_num = (__int128_t)hash_num * (__int128_t)(_num_lits) + (__int128_t)lit_idx + (__int128_t)_num_lits;
        }
        return hash_num;
    }
    bool ls_solver::is_same_lits(std::vector<int> &lits_1, std::vector<int> &lits_2)
    {
        if (lits_1.size() != lits_2.size())
        {
            return false;
        }
        std::sort(lits_1.begin(), lits_1.end());
        std::sort(lits_2.begin(), lits_2.end());
        for (int l_idx = 0; l_idx < lits_1.size(); l_idx++)
        {
            if (lits_1[l_idx] != lits_2[l_idx])
            {
                return false;
            }
        }
        return true;
    }

    void ls_solver::resolution()
    {
        std::vector<uint64_t> pos_clauses(10 * _num_clauses);
        std::vector<uint64_t> neg_clauses(10 * _num_clauses);
        std::map<__int128_t, int> clauselit_map;            // for the clause with literal {a,b,c}, sort the lit by its order, and hash the literals to a number, then map it to the clause_idx, if deleted, set it to -1
        std::vector<__int128_t> clauselit(_clauses.size()); // hash the lits of clause to a number
        for (int cls_idx = 0; cls_idx < _clauses.size(); cls_idx++)
        {
            clauselit[cls_idx] = hash_lits_to_num(_clauses[cls_idx].literals);
            clauselit_map[clauselit[cls_idx]] = cls_idx;
        }
        int pos_clause_size, neg_clause_size;
        bool is_improve = true;
        while (is_improve)
        {
            is_improve = false;
            for (uint64_t bool_var_idx : bool_var_vec)
            {
                if (_vars[bool_var_idx].is_delete)
                    continue;
                pos_clause_size = 0;
                neg_clause_size = 0;
                for (int i = 0; i < _vars[bool_var_idx].clause_idxs.size(); i++)
                {
                    uint64_t clause_idx = _vars[bool_var_idx].clause_idxs[i];
                    if (_clauses[clause_idx].is_delete)
                        continue;
                    for (int l_var_sign : _clauses[clause_idx].literals)
                    {
                        if (!_lits[std::abs(l_var_sign)].is_nia_lit && _lits[std::abs(l_var_sign)].delta == bool_var_idx)
                        { // make sure that it is a boolean literal and is exactly the one containing the var
                            if (l_var_sign > 0)
                            {
                                pos_clauses[pos_clause_size++] = clause_idx;
                            }
                            else
                            {
                                neg_clauses[neg_clause_size++] = clause_idx;
                            }
                            break;
                        }
                    }
                } // determine the pos_clause and neg_clause
                int tautology_num = 0;
                for (int i = 0; i < pos_clause_size; i++)
                { // pos clause X neg clause
                    uint64_t pos_clause_idx = pos_clauses[i];
                    for (int j = 0; j < neg_clause_size; j++)
                    {
                        uint64_t neg_clause_idx = neg_clauses[j];
                        bool is_tautology_flag = false;
                        for (int k = 0; k < _clauses[neg_clause_idx].literals.size(); k++)
                        {
                            int l_neg_lit = _clauses[neg_clause_idx].literals[k];
                            if (_lits[std::abs(l_neg_lit)].delta != bool_var_idx || _lits[std::abs(l_neg_lit)].is_nia_lit)
                            { // the bool_var for resolution is not considered(that is \neg ( the lit is bool lit and it contains the var))
                                for (int l_pos_lit : _clauses[pos_clause_idx].literals)
                                {
                                    if (-l_neg_lit == (l_pos_lit))
                                    {
                                        is_tautology_flag = true;
                                        break;
                                    } // if there exists (aVb)^(-aV-b), the new clause is tautology
                                }
                            }
                            if (is_tautology_flag)
                            {
                                break;
                            }
                        }
                        if (is_tautology_flag)
                        {
                            tautology_num++;
                        }
                    }
                }
                if ((pos_clause_size * neg_clause_size - tautology_num) > (pos_clause_size + neg_clause_size))
                {
                    continue;
                } // if deleting the var can cause 2 times clauses, then skip it
                for (uint64_t clause_idx : _vars[bool_var_idx].clause_idxs)
                { // delete the clauses of bool_var
                    _clauses[clause_idx].is_delete = true;
                    for (int l_idx_sign : _clauses[clause_idx].literals)
                    { // delete the clause from corresponding bool var
                        lit *l = &(_lits[std::abs(l_idx_sign)]);
                        if (!l->is_nia_lit && l->delta != bool_var_idx)
                        {
                            variable *var_inner = &(_vars[l->delta.to_int()]);
                            for (uint64_t delete_idx = 0; delete_idx < var_inner->clause_idxs.size(); delete_idx++)
                            {
                                if (var_inner->clause_idxs[delete_idx] == clause_idx)
                                {
                                    var_inner->clause_idxs[delete_idx] = var_inner->clause_idxs.back();
                                    var_inner->clause_idxs.pop_back();
                                    break;
                                }
                            }
                        }
                    }
                }
                is_improve = true;
                _vars[bool_var_idx].is_delete = true;
                if (pos_clause_size == 0)
                {
                    _vars[bool_var_idx].up_bool = -1;
                } // if it is a false pure lit, the var is set to false
                if (neg_clause_size == 0)
                {
                    _vars[bool_var_idx].up_bool = 1;
                } // if it is a true pure lit, the var is set to true
                if (pos_clause_size == 0 || neg_clause_size == 0)
                    continue; // pos or neg clause is empty, meaning the clauses are SAT when assigned to true or false, then cannot resolute, just delete the clause
                for (int i = 0; i < pos_clause_size; i++)
                { // pos clause X neg clause
                    uint64_t pos_clause_idx = pos_clauses[i];
                    for (int j = 0; j < neg_clause_size; j++)
                    {
                        uint64_t neg_clause_idx = neg_clauses[j];
                        clause new_clause;
                        uint64_t pos_lit_size = _clauses[pos_clause_idx].literals.size();
                        uint64_t neg_lit_size = _clauses[neg_clause_idx].literals.size();
                        new_clause.literals.reserve(pos_lit_size + neg_lit_size);
                        bool is_tautology = false;
                        for (int k = 0; k < pos_lit_size; k++)
                        {
                            int l_sign_idx = _clauses[pos_clause_idx].literals[k];
                            if (_lits[std::abs(l_sign_idx)].is_nia_lit || _lits[std::abs(l_sign_idx)].delta != bool_var_idx)
                            {
                                new_clause.literals.push_back(l_sign_idx);
                            }
                        } // add the lits in pos clause to new clause
                        for (int k = 0; k < neg_lit_size; k++)
                        {
                            int l_sign_idx = _clauses[neg_clause_idx].literals[k];
                            if (_lits[std::abs(l_sign_idx)].is_nia_lit || _lits[std::abs(l_sign_idx)].delta != bool_var_idx)
                            {
                                bool is_existed_lit = false;
                                for (uint64_t i = 0; i < pos_lit_size - 1; i++)
                                {
                                    if (l_sign_idx == (new_clause.literals[i]))
                                    {
                                        is_existed_lit = true;
                                        break;
                                    } // if the lit has existed, then it will not be added
                                    if (l_sign_idx == (-new_clause.literals[i]))
                                    {
                                        is_tautology = true;
                                        break;
                                    } // if there exists (aVb)^(-aV-b), the new clause is tautology
                                }
                                if (is_tautology)
                                {
                                    break;
                                }
                                if (!is_existed_lit)
                                {
                                    new_clause.literals.push_back(l_sign_idx);
                                }
                            }
                        }
                        if (!is_tautology)
                        {
                            __int128_t clause_lit_hash = hash_lits_to_num(new_clause.literals);
                            bool should_add = false;
                            if (clauselit_map.find(clause_lit_hash) == clauselit_map.end())
                            {
                                should_add = true;
                            } // the clause never appears
                            else
                            {
                                int origin_clause_idx = clauselit_map[clause_lit_hash];
                                clause *cl_origin = &(_clauses[origin_clause_idx]);
                                if (cl_origin->is_delete)
                                {
                                    should_add = true;
                                } // the clause has been deleted
                                else if (!is_same_lits(cl_origin->literals, new_clause.literals))
                                {
                                    should_add = true;
                                } // not the same one
                            }
                            if (should_add)
                            { // add new clause, if it never appers then add it
                                for (int l_sign_idx : new_clause.literals)
                                {
                                    lit *l_inner = &(_lits[std::abs(l_sign_idx)]);
                                    if (!l_inner->is_nia_lit)
                                    {
                                        _vars[l_inner->delta.to_int()].clause_idxs.push_back((int)_num_clauses);
                                    }
                                }
                                _clauses.push_back(new_clause);
                                clauselit.push_back(clause_lit_hash);
                                clauselit_map[clause_lit_hash] = (int)_num_clauses;
                                _num_clauses++;
                            }
                        }
                    }
                }
                for (int i = 0; i < pos_clause_size; i++)
                {
                    clause pos_cl = _clauses[pos_clauses[i]];
                    for (int j = 0; j < pos_cl.literals.size(); j++)
                    {
                        int l_idx = pos_cl.literals[j];
                        lit *l = &(_lits[std::abs(l_idx)]);
                        if (!l->is_nia_lit && l->delta == bool_var_idx)
                        {
                            pos_cl.literals[j] = pos_cl.literals[0];
                            pos_cl.literals[0] = l_idx;
                            break;
                        }
                    }
                    _reconstruct_stack.push_back(pos_cl);
                }
                for (int i = 0; i < neg_clause_size; i++)
                {
                    clause neg_cl = _clauses[neg_clauses[i]];
                    for (int j = 0; j < neg_cl.literals.size(); j++)
                    {
                        int l_idx = neg_cl.literals[j];
                        lit *l = &(_lits[std::abs(l_idx)]);
                        if (!l->is_nia_lit && l->delta == bool_var_idx)
                        {
                            neg_cl.literals[j] = neg_cl.literals[0];
                            neg_cl.literals[0] = l_idx;
                            break;
                        }
                    }
                    _reconstruct_stack.push_back(neg_cl);
                }
            }
        }
    }
    bool cmp_vl_by_var(const var_lit &vl1, const var_lit &vl2) { return vl1.var_idx < vl2.var_idx; }
    bool cmp_vl_by_lit(const var_lit &vl1, const var_lit &vl2) { return vl1.lit_idx < vl2.lit_idx; }

    // prepare coff var, determine all fa_coffs
    void ls_solver::prepare_coff_var()
    {
        for (int idx = 0; idx < _vars.size(); idx++)
        {
            if (_vars[idx].is_nia)
                find(idx);
        }
    }
    // turn the coffs and key of the lit to integer value
    void ls_solver::gcd_for_lit(lit &l)
    {
        if (l.lits_index <= 0 || !l.is_nia_lit || l.coff_vars.size() <= 1)
            return;
        int64_t m_total = l.key.m;
        for (const auto &cv : l.coff_vars)
            m_total *= cv.coff.m;
        for (auto &cv : l.coff_vars)
            cv.coff *= m_total;
        l.key *= m_total;
        int64_t result = l.coff_vars[0].coff.n;
        for (const auto &cv : l.coff_vars)
        {
            result = std::gcd(result, cv.coff.n);
            if (result == 1)
                return;
        }
        result = std::gcd(result, l.key.n);
        if (result != 1)
        {
            for (size_t idx = 0; idx < l.coff_vars.size(); idx++)
            {
                l.coff_vars[idx].coff /= result;
            }
            l.key /= result;
        }
    }

    void ls_solver::determine_lit_appear()
    {
        for (int idx = 0; idx < lit_appear.size(); idx++)
            lit_appear[idx] = false;
        lits_in_cls->clear();
        for (const auto &c : _clauses)
        {
            for (const auto &l : c.literals)
            {
                int l_idx = std::abs(l);
                lit_appear[l_idx] = true;
                lits_in_cls->insert_element(l_idx);
            }
        }
    }

    void ls_solver::reduce_duplicated_lits(bool &modified)
    {
        if (!lits_been_modified)
            return;
        std::vector<int> same_lit(_num_lits, 0); // same_lit[i] = j means lit i and j are the same lit
        int lits_in_cls_num = lits_in_cls->size();
        for (int curr_idx = 1; curr_idx < lits_in_cls_num; curr_idx++)
        {
            int l_idx_curr = lits_in_cls->element_at(curr_idx);
            if (!_lits[l_idx_curr].is_nia_lit)
                continue;
            for (int pre_idx = 0; pre_idx < curr_idx; pre_idx++)
            {
                int l_idx_pre = lits_in_cls->element_at(pre_idx);
                if (same_lit[l_idx_pre] != 0 || !_lits[l_idx_pre].is_nia_lit)
                    continue; // the pre lit is same to previous ones, no more consider it
                if (is_same_lit(_lits[l_idx_curr], _lits[l_idx_pre]))
                {
                    same_lit[l_idx_curr] = l_idx_pre;
                    break;
                } // find the first same lit
            }
        }
        // replace the lits in clauses
        for (auto &c : _clauses)
        {
            if (c.is_delete)
                continue;
            for (auto &l_idx : c.literals)
            {
                lit *l = &_lits[std::abs(l_idx)];
                if (l->is_nia_lit && l->lits_index != 0)
                {
                    int abs_l_idx = std::abs(l_idx);
                    if (same_lit[abs_l_idx] != 0)
                    {
                        modified = true;
                        if (l_idx > 0)
                            l_idx = same_lit[abs_l_idx];
                        else
                            l_idx = -same_lit[abs_l_idx];
                    }
                }
            }
        }
        lits_been_modified = false;
    }

    // all lits in m_lits are unit equalities with multiple items
    bool ls_solver::resolve_multiple_equals(const std::vector<int> &m_lits)
    {
        if (m_lits.size() < 2)
            return false;
        std::map<int, size_t> var_hash;
        std::vector<int> var_idxs;
        for (const auto &l_idx : m_lits)
        {
            lit &l = _lits[l_idx];
            for (const auto &cv : l.coff_vars)
            {
                int var_idx = cv.var_idx;
                if (var_hash.find(var_idx) == var_hash.end())
                {
                    var_hash[var_idx] = var_idxs.size();
                    var_idxs.push_back(var_idx);
                }
            }
        }
        size_t row_num = m_lits.size();
        size_t collom_num = var_idxs.size() + 1;
        std::vector<std::vector<ration_num>> matrix(row_num);
        for (auto &row : matrix)
            row.resize(collom_num, 0);
        for (size_t l_idx = 0; l_idx < row_num; l_idx++)
        {
            lit &l = _lits[m_lits[l_idx]];
            for (const auto &cv : l.coff_vars)
            {
                size_t hash_val = var_hash[cv.var_idx];
                matrix[l_idx][hash_val] = cv.coff;
            }
            matrix[l_idx][collom_num - 1] = l.key;
        }
        // now the lits has been converted to matrix, matrix[i][j]=c indicates that in i_th literal, the coff of terms[j] is c, the last collom of the matrix indicates the key of literals
        gaussianElimination(matrix);
        if (!contains_short_row(matrix))
        {
            return false; // if after the modification, there is still no lit with items less than 2
        }
        for (size_t r_idx = 0; r_idx < row_num; r_idx++)
        {
            lit &l = _lits[m_lits[r_idx]];
            if (is_all_zero_row(matrix[r_idx]))
                l.lits_index = 0;
            else
            {
                l.coff_vars.clear();
                for (size_t c_idx = 0; c_idx < collom_num - 1; c_idx++)
                {
                    if (matrix[r_idx][c_idx] != 0)
                    {
                        int var_idx = var_idxs[c_idx];
                        ration_num coff = matrix[r_idx][c_idx];
                        l.coff_vars.push_back(coff_var(var_idx, coff));
                    }
                }
                l.key = matrix[r_idx][collom_num - 1];
            }
        }
        return true;
    }

    // 判断文字是否可以因为变量的界而变为true或false
    void ls_solver::determine_lits_by_bound(bool &modified)
    {
        // 遍历所有文字，如果该文字是nia的，则看其中的各个变量是否都有界，如果都有界，则该文字也有界，看是否已经
        for (int idx = 0; idx < lits_in_cls->size(); idx++)
        {
            lit &l = _lits[lits_in_cls->element_at(idx)];
            if (!l.is_nia_lit || l.lits_index == 0 || l.lits_index < 0)
                continue;
            ration_num lit_up_bound = l.key;
            for (const auto &cv : l.coff_vars)
            {
                variable &var = _vars[cv.var_idx];
                if (cv.coff > 0 && var.upper_bound != max_int)
                    lit_up_bound += cv.coff * var.upper_bound;
                else if (cv.coff < 0 && var.low_bound != -max_int)
                    lit_up_bound += cv.coff * var.low_bound;
                else
                {
                    lit_up_bound = max_int;
                    break;
                }
            }
            ration_num lit_low_bound = l.key;
            for (const auto &cv : l.coff_vars)
            {
                variable &var = _vars[cv.var_idx];
                if (cv.coff > 0 && var.low_bound != -max_int)
                    lit_low_bound += cv.coff * var.low_bound;
                else if (cv.coff < 0 && var.upper_bound != max_int)
                    lit_low_bound += cv.coff * var.upper_bound;
                else
                {
                    lit_low_bound = -max_int;
                    break;
                }
            }
            if (!l.is_equal)
            {
                if (lit_up_bound < lit_low_bound || lit_low_bound > 0)
                {
                    l.lits_index = -1;
                    l.is_true = false;
                    modified = true;
                }
                else if (lit_up_bound <= 0)
                {
                    l.lits_index = 0;
                    modified = true;
                }
            }
            else // equal literal
            {
                if (lit_up_bound == lit_low_bound)
                {
                    if (lit_up_bound != 0)
                    {
                        l.lits_index = -1;
                        l.is_true = false;
                        modified = true;
                    }
                    else
                    {
                        l.lits_index = 0;
                        modified = true;
                    }
                }
                else if (lit_up_bound < 0 || lit_low_bound > 0)
                {
                    l.lits_index = -1;
                    l.is_true = false;
                    modified = true;
                }
            }
        }
    }

    // 删除多余的子句
    // 1. 删除子句中的重复文字（这里的重复是指2个文字虽然编号不同，但是内容是一致的）
    // 2. 某些子句已经包含true literal
    // 3. 删除同一个子句中的重复文字，删除子句中的假文字
    // 4. 删除重复的子句，以及其中包含出现于单元子句中文字的子句
    void ls_solver::reduce_clause(bool &modified)
    {
        reduce_duplicated_lits(modified);  // 删除重复的文字，即编号不同但是实际一样的
        determine_lits_by_bound(modified); // 根据var的bound确定文字是否已经为true或false
        int reduced_clause_num = 0;
        uint64_t original_clause_num = _num_clauses;
        // delete clause that has been marked deleted and those containing true literal
        for (int i = 0; i < _num_clauses; i++)
        {
            if (!_clauses[i].is_delete)
            {
                bool is_tautology = false;
                for (const auto &l_idx : _clauses[i].literals)
                {
                    if ((_lits[std::abs(l_idx)].lits_index == 0 && l_idx > 0) || (_lits[std::abs(l_idx)].lits_index < 0 && l_idx < 0))
                    {
                        is_tautology = true;
                        break;
                    }
                }
                if (!is_tautology)
                    _clauses[reduced_clause_num++] = _clauses[i];
            }
        }
        _clauses.resize(reduced_clause_num);
        _num_clauses = reduced_clause_num;
        // delete the duplicated literals in the same clause
        for (auto &c : _clauses)
        {
            std::sort(c.literals.begin(), c.literals.end());
            unsigned clause_sz = 0;
            for (int l : c.literals)
            {
                if ((_lits[std::abs(l)].lits_index == 0 && l < 0) || (_lits[std::abs(l)].lits_index < 0 && l > 0))
                {
                    continue;
                } // the lit is already false
                if (clause_sz == 0 || c.literals[clause_sz - 1] != l)
                {
                    c.literals[clause_sz++] = l;
                }
            }
            if (c.literals.size() != clause_sz)
                modified = true;
            c.literals.resize(clause_sz);
            if (clause_sz == 0)
                build_unsat = true;
        }
        // delete redundant clauses
        std::vector<std::vector<int>> clause_vec(_num_clauses);
        for (size_t c_idx = 0; c_idx < _clauses.size(); c_idx++)
            clause_vec[c_idx] = _clauses[c_idx].literals;
        delete_redundant_clauses(clause_vec);
        _num_clauses = clause_vec.size();
        _clauses.resize(_num_clauses);
        for (size_t c_idx = 0; c_idx < _num_clauses; c_idx++)
            _clauses[c_idx].literals = clause_vec[c_idx];
        if (original_clause_num != _num_clauses)
            modified = true;
        determine_lit_appear();
    }

    void ls_solver::prepare_cls_lit_idx_for_vars()
    {
        // var_idx of all terms appear in lits are transfered to reduced vars
        // the lits_idx == 0 means the NIA lit is true
        // transfer the resolution vars to vars

        best_found_cost = (int)_num_clauses;
        for (size_t idx = 0; idx < lit_appear.size(); idx++)
            lit_appear[idx] = false;
        var_appear.resize(_vars.size() + _additional_len);
        std::fill(var_appear.begin(), var_appear.end(), false);
        // clear the clause idxs of bool vars
        for (const auto &bool_v_idx : bool_var_vec)
        {
            _vars[bool_v_idx].clause_idxs.clear();
        }
        int reduced_clause_num = (int)_clauses.size();
        for (int clause_idx = 0; clause_idx < reduced_clause_num; clause_idx++)
        {
            _clauses[clause_idx].weight = 1;
            for (int k = 0; k < _clauses[clause_idx].literals.size(); k++)
            {
                int l_sign_idx = _clauses[clause_idx].literals[k];
                lit *l = &(_lits[std::abs(l_sign_idx)]);
                if (l->is_nia_lit)
                {
                    for (const auto &cv : l->coff_vars)
                    {
                        if (!var_appear[cv.var_idx])
                        {
                            var_appear[cv.var_idx] = true;
                        }
                        _vars[cv.var_idx].clause_idxs.push_back(clause_idx);
                    }
                    _clauses[clause_idx].nia_literals.push_back(l_sign_idx);
                }
                else
                {
                    if (!lit_appear[std::abs(l_sign_idx)])
                    {
                        _vars[l->delta.to_int()].literal_idxs.push_back(std::abs(l_sign_idx)); // for a boolean var, its first lit_idx is the lit containing the var
                    }
                    _vars[l->delta.to_int()].clause_idxs.push_back(clause_idx);
                    _vars[l->delta.to_int()].bool_var_in_pos_clause.push_back(l_sign_idx > 0); // for a boolean var, if it is neg in a clause, it is false
                    _clauses[clause_idx].bool_literals.push_back(l_sign_idx);
                    if (!var_appear[l->delta.to_int()])
                        var_appear[l->delta.to_int()] = true;
                }
                if (!lit_appear[std::abs(l_sign_idx)])
                {
                    lit_appear[std::abs(l_sign_idx)] = true;
                }
            }
        }
        for (variable &v : _vars)
        {
            uint64_t pre_clause_idx = INT64_MAX;
            int var_clause_num = 0;
            for (int i = 0; i < v.clause_idxs.size(); i++)
            {
                uint64_t tmp_clause_idx = v.clause_idxs[i];
                if (pre_clause_idx != tmp_clause_idx)
                {
                    pre_clause_idx = tmp_clause_idx;
                    v.clause_idxs[var_clause_num++] = (int)pre_clause_idx;
                }
            }
            v.clause_idxs.resize(var_clause_num);
        } // determine the clause_idxs of var
        lit *l;
        for (unsigned l_idx = 0; l_idx < _lits.size(); l_idx++)
        {
            l = &(_lits[l_idx]);
            if (!lit_appear[l_idx])
            {
                continue;
            }
            for (int pos_var_idx = 0; pos_var_idx < l->coff_vars.size(); pos_var_idx++)
            {
                uint64_t var_idx = l->coff_vars[pos_var_idx].var_idx;
                ration_num coff = l->coff_vars[pos_var_idx].coff;
                var_lit vl(var_idx, l_idx, coff);
                _vars[var_idx].var_lits.push_back(vl);
                l->var_lits.push_back(vl);
            }
        } // determine the var_lit of var and lit, the vlt has been sorted by the lit_idx in vars
        for (unsigned l_idx = 0; l_idx < _lits.size(); l_idx++)
        {
            l = &(_lits[l_idx]);
            if (lit_appear[l_idx])
            {
                std::sort(l->var_lits.begin(), l->var_lits.end(), cmp_vl_by_var);
            }
        } // sort the vlt in lit by its var_idx
        for (variable &v : _vars)
        {
            uint64_t pre_lit_idx = INT64_MAX;
            int var_lit_num = 0;
            for (int i = 0; i < v.var_lits.size(); i++)
            {
                uint64_t tmp_lit_idx = v.var_lits[i].lit_idx;
                if (pre_lit_idx != tmp_lit_idx)
                {
                    var_lit_num++;
                    pre_lit_idx = tmp_lit_idx;
                    v.literal_idxs.push_back(pre_lit_idx);
                }
            }
            v.literal_idxs.resize(var_lit_num);
        } // determine the lit_idxs of var
        prepare_coff_var();
        prepare_up_value_vars();
        b_width_idx = name2var[basic_component_name + "_width"];
        b_hight_idx = name2var[basic_component_name + "_hight"];
    }

    bool ls_solver::is_same_lit(const lit &l1, const lit &l2)
    {
        if ((!l1.is_nia_lit) || (!l2.is_nia_lit) || (l1.is_equal != l2.is_equal) || (l1.key != l2.key) || (l1.coff_vars.size() != l2.coff_vars.size()))
            return false;
        for (size_t idx = 0; idx < l1.coff_vars.size(); idx++)
        {
            if ((l1.coff_vars[idx].coff != l2.coff_vars[idx].coff) || (l1.coff_vars[idx].var_idx != l2.coff_vars[idx].var_idx))
                return false;
        }
        return true;
    }

    bool ls_solver::is_neg_lit(const lit &l1, const lit &l2)
    {
        if (l1.is_equal || l2.is_equal)
            return false;
        if (!l1.is_nia_lit || !l2.is_nia_lit)
            return false;
        if (l1.coff_vars.size() != l2.coff_vars.size() || l1.key != -l2.key)
            return false;
        for (size_t idx = 0; idx < l1.coff_vars.size(); idx++)
        {
            if ((l1.coff_vars[idx].coff != -l2.coff_vars[idx].coff) || (l1.coff_vars[idx].var_idx != l2.coff_vars[idx].var_idx))
                return false;
        }
        return true;
    }

    void ls_solver::prepare_components_idx()
    {
        std::string v_name, val, v_end;
        std::map<std::string, int> kids_names2fre;
        for (const auto &nia_v_idx : nia_var_vec)
        {
            v_name = _vars[nia_v_idx].var_name;
            size_t pos = v_name.find_last_of('.');
            if (pos != std::string::npos)
            {
                v_end = v_name.substr(pos + 1);
                v_name = v_name.substr(0, pos);
            }
            if ((v_end == "x" || v_end == "y" || v_end == "w" || v_end == "h") && (v_name != basic_component_name))
            {
                if (kids_names2fre.find(v_name) != kids_names2fre.end())
                    kids_names2fre[v_name]++;
                else
                    kids_names2fre[v_name] = 1;
            }
        }
        for (const auto &pair : kids_names2fre)
        {
            if (pair.second == 4)
                add_component(pair.first);
        }
        add_component(basic_component_name);
    }

    void ls_solver::add_component(const std::string &c_name)
    {
        components_idx.push_back(name2var[c_name + ".w"]);
        components_idx.push_back(name2var[c_name + ".h"]);
        components_idx.push_back(name2var[c_name + ".x"]);
        components_idx.push_back(name2var[c_name + ".y"]);
        // components_idx.push_back(name2var[c_name + "_feasible"]);
        component_names.push_back(c_name);
    }

    void ls_solver::prepare_soft_components_idx(const std::vector<std::string> &soft_c_names)
    {
        for (const auto &soft_c_name : soft_c_names)
        {
            soft_components_idx.push_back(name2var[soft_c_name + ".w"]);
            soft_components_idx.push_back(name2var[soft_c_name + ".h"]);
            soft_components_idx.push_back(name2var[soft_c_name + ".x"]);
            soft_components_idx.push_back(name2var[soft_c_name + ".y"]);
            // soft_components_idx.push_back(name2var[soft_c_name + "_feasible"]);
        }
    }

    void ls_solver::make_space()
    {
        // about num lits (not change)
        _lit_make_break.resize(_num_lits + _additional_len);
        is_in_unsat_clause.resize(_num_lits + _additional_len);
        lits_in_unsat_clause.resize(_num_lits + _additional_len);
        if (false_lit_occur == NULL)
            false_lit_occur = new Array((int)_num_lits + (int)_additional_len);

        // about num_opt (not change)
        operation_var_idx_vec.resize(3 * _num_opt + _additional_len);
        operation_var_idx_bool_vec.resize(3 * _num_opt + _additional_len);
        operation_change_value_vec.resize(3 * _num_opt + _additional_len);

        // about num clause (change)
        if (unsat_clauses != NULL)
            delete unsat_clauses;
        unsat_clauses = new Array((int)_num_clauses + (int)_additional_len);
        if (sat_clause_with_false_literal != NULL)
            delete sat_clause_with_false_literal;
        sat_clause_with_false_literal = new Array((int)_num_clauses + (int)_additional_len);
        if (contain_bool_unsat_clauses != NULL)
            delete contain_bool_unsat_clauses;
        contain_bool_unsat_clauses = new Array((int)_num_clauses);

        // about num vars (not change)
        _solution.resize(2 * _num_vars + _additional_len);
        _best_solutin.resize(_num_vars + _additional_len);
        tabulist.resize(2 * _num_vars + _additional_len);
        last_move.resize(2 * _num_vars + _additional_len);
        is_chosen_bool_var.resize(_num_vars + _additional_len);
    }

    void ls_solver::set_pre_value()
    {
        _pre_value_1.resize(_num_vars + _additional_len);
        _pre_value_2.resize(_num_vars + _additional_len);
        std::fill(_pre_value_1.begin(), _pre_value_1.end(), INT32_MAX);
        std::fill(_pre_value_2.begin(), _pre_value_2.end(), INT32_MAX);
        for (clause &cl : _clauses)
        {
            if (cl.literals.size() == 1 && cl.literals[0] > 0 && _lits[cl.literals[0]].is_equal)
            {
                lit *l = &(_lits[cl.literals[0]]);
                if (l->coff_vars.size() == 1)
                {
                    int var_idx = l->coff_vars[0].var_idx;
                    _pre_value_1[var_idx] = (-l->key / l->coff_vars[0].coff);
                }
            } //(a==0)
            else if (cl.literals.size() == 2 && cl.literals[0] > 0 && _lits[cl.literals[0]].is_equal && cl.literals[1] > 0 && _lits[cl.literals[1]].is_equal)
            {
                lit *l1 = &(_lits[cl.literals[0]]);
                lit *l2 = &(_lits[cl.literals[1]]);
                if ((l1->coff_vars.size() == 1) && (l2->coff_vars.size() == 1) && (l1->coff_vars[0].var_idx == l2->coff_vars[0].var_idx))
                {
                    int var_idx = l1->coff_vars[0].var_idx;
                    _pre_value_1[var_idx] = (-l1->key / l1->coff_vars[0].coff);
                    _pre_value_2[var_idx] = (-l2->key / l2->coff_vars[0].coff);
                }
            } //(a==0 OR a==1)
        }
    }
    ls_solver::~ls_solver()
    {
        if (unsat_clauses != NULL)
            delete unsat_clauses;
        if (sat_clause_with_false_literal != NULL)
            delete sat_clause_with_false_literal; // clauses with 0<sat_num<literal_num, from which swap operation are choosen
        if (contain_bool_unsat_clauses != NULL)
            delete contain_bool_unsat_clauses; // unsat clause with at least one boolean var
        if (false_lit_occur != NULL)
            delete false_lit_occur; // the false lits for choosing critical move
        if (lits_in_cls != NULL)
            delete lits_in_cls;
    }
}
