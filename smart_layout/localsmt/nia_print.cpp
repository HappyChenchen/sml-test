#include "nia_ls.h"
#include <set>
#include <sstream>
#define LS_DEBUG
namespace nia_overall
{
    // print
    void ls_solver::print_formula()
    {
        for (int i = 0; i < _num_clauses; i++)
        {
            clause *cl = &(_clauses[i]);
            std::cout << i << "\n";
            for (int l_idx : cl->literals)
            {
                std::cout << l_idx << ": ";
                if (l_idx < 0)
                {
                    std::cout << "neg: ";
                }
                print_literal(_lits[std::abs(l_idx)]);
            }
            std::cout << "\n";
        }
    }

    void ls_solver::print_formula_smt()
    {
        std::cout << "(set-info :smt-lib-version 2.6)\n(set-logic QF_IDL)\n";
        for (variable &v : _vars)
        {
            std::cout << "(declare-fun " << v.var_name << " () " << (v.is_nia ? "Int" : "Bool") << ")\n";
        }
        std::cout << "(assert\n(and\n";
        for (clause &cl : _clauses)
        {
            std::cout << "(or ";
            for (int lit_idx : cl.literals)
            {
                print_lit_smt(lit_idx);
            }
            std::cout << ")\n";
        }
        std::cout << "))\n(check-sat)\n(exit)\n";
    }

    void ls_solver::print_literal(lit &l)
    {
        if (!l.is_nia_lit)
        {
            std::cout << _vars[l.delta.to_int()].var_name << "\n";
        }
        else
        {
            for (const auto &cv : l.coff_vars)
            {
                std::cout << "( " << (cv.coff).print() << " * " << _vars[cv.var_idx].var_name << " ) + ";
            }
            std::cout << "( " << (l.key).print() << " ) " << (l.is_equal ? "==" : "<=") << " 0\n";
        }
    }

    void ls_solver::print_lit_smt(int lit_idx)
    {
        lit *l = &(_lits[std::abs(lit_idx)]);
        if (l->is_nia_lit)
        {
            // TODO:
        }
        else
        {
            if (lit_idx > 0)
            {
                std::cout << _vars[l->delta.to_int()].var_name << " ";
            }
            else
            {
                std::cout << "(" << " not " << _vars[l->delta.to_int()].var_name << " ) ";
            }
        }
    }

    void ls_solver::print_mv()
    {
        std::cout << "(model\n";
        for (uint64_t var_idx = 0; var_idx < _num_vars; var_idx++)
        {
            print_mv_vars(var_idx);
        }
        std::cout << ")\n";
    }

    void ls_solver::print_components(int offset_x, int offset_y, std::ostream &out)
    {

        const size_t component_num = component_names.size();
        for (size_t c_idx = 0; c_idx < component_num - 1; c_idx++)
        {
            out<<",\n";
            // if (_solution[components_idx[5 * c_idx + 4]] < 0)
            // {
            //     out << R"({ "name": ")" << component_names[c_idx]
            //         << R"(", "width":0, "height":0, "x": 0, "y": 0, "display":"hidden"})";
            // }
            // else
            {
                out << "{ \"name\": \"" << component_names[c_idx] << "\"";
                out << ", \"width\": " << _solution[components_idx[4 * c_idx]].print();
                out << ", \"height\": " << _solution[components_idx[4 * c_idx + 1]].print();
                out << ", \"x\": " << (_solution[components_idx[4 * c_idx + 2]] + offset_x).print();
                out << ", \"y\": " << (_solution[components_idx[4 * c_idx + 3]] + offset_y).print();
                out << " , \"display\": \"block\" }";
            }
        }
    }

    void ls_solver::print_component(double &x, double &y, double &w, double &h, int &v, int c_idx, int offset_x, int offset_y)
    {
        // if (_solution[components_idx[5 * c_idx + 4]] < 0)
        // {
        //     v = 0;
        // }
        // else
        {
            v = 1;
            w = _solution[components_idx[4 * c_idx]].to_double();
            h = _solution[components_idx[4 * c_idx + 1]].to_double();
            x = _solution[components_idx[4 * c_idx + 2]].to_double() + offset_x;
            y = _solution[components_idx[4 * c_idx + 3]].to_double() + offset_y;
        }
    }

    void ls_solver::print_full_model()
    {
        std::cout << "var rectangles = [\n";
        print_components();
        print_end();
    }

    void ls_solver::print_end()
    {
        std::cout << R"(];
rectangles.forEach(function (rect) {
    var div = document.createElement('div');
    div.className = 'rectangle';
    div.style.width = rect.width + 'px';
    div.style.height = rect.height + 'px';
    div.style.left = rect.x + 'px';
    div.style.top = rect.y + 'px';
    div.style.display = rect.display;
    document.body.appendChild(div);
    var textDiv = document.createElement("div");
    textDiv.className = "text";
    textDiv.textContent = rect.name;
    div.appendChild(textDiv);
    document.body.appendChild(div);
});)";
        std::cout << std::endl;
    }

    void ls_solver::print_mv_vars(uint64_t var_idx)
    {
        variable *v = &(_vars[var_idx]);
        ration_num var_solution = _solution[var_idx];
        std::cout << "  (define-fun " << v->var_name << " () ";
        if (v->is_nia)
        {
            std::cout << "Int ";
            if (var_solution >= 0)
            {
                std::cout << (var_solution).print() << ")\n";
            }
            else
            {
                std::cout << "(- " << (-var_solution).print() << "))\n";
            }
        }
        else
        {
            std::cout << "Bool ";
            if (var_solution > 0)
            {
                std::cout << "true )\n";
            }
            else
            {
                std::cout << "false )\n";
            }
        }
    }

    void ls_solver::print_var_solution(std::string &var_name, std::string &var_value)
    {
        uint64_t var_idx = name2var[var_name];
        if (_vars[var_idx].is_nia)
            var_value = _solution[var_idx].print();
        else
            var_value = _solution[var_idx] > 0 ? "block" : "none";
        return;
    } // the var exists in _vars

    bool ls_solver::print_var_solution(std::string &var_name, ration_num &var_value)
    {
        uint64_t var_idx = name2var[var_name];
        var_value = _solution[var_idx];
        return true;
    }

    std::string ls_solver::print_128(__int128_t n)
    {
        std::stringstream ss;
        if (n == 0)
            return "0";
        if (n < 0)
        {
            ss << "-";
            n = -n;
        }
        int a[50], ai = 0;
        memset(a, 0, sizeof a);
        while (n != 0)
        {
            a[ai++] = n % 10;
            n /= 10;
        }
        for (int i = 1; i <= ai; i++)
            ss << abs(a[ai - i]);
        return ss.str();
    }
    void ls_solver::up_bool_vars()
    {
        for (size_t v_idx = 0; v_idx < _num_vars; v_idx++)
        {
            if (!var_appear[v_idx])
            {
                if (_vars[v_idx].is_nia)
                {
                    int root_idx = fa[v_idx];
                    if (root_idx != v_idx)
                        _solution[v_idx] = _solution[root_idx] * fa_coff[v_idx] + fa_const[v_idx];
                }
                else if (up_value_vars[v_idx] == 0)
                    _solution[v_idx] = -1;
            }
        }
        std::vector<bool> lit_appear_tmp = lit_appear;
        // set value for those vars not appearing in formula
        for (int idx = (int)_reconstruct_stack.size() - 1; idx >= 0; idx--)
        {
            clause cl = _reconstruct_stack[idx];
            bool sat_flag = false;
            for (int l_idx : cl.literals)
            {
                lit *l = &(_lits[std::abs(l_idx)]);
                if (l_idx > 0 && l->lits_index == 0)
                    sat_flag = true;
                else if (l->is_nia_lit)
                {
                    if (!lit_appear_tmp[std::abs(l_idx)])
                    {
                        ration_num delta = delta_lit(_lits[std::abs(l_idx)]);
                        l->delta = delta;
                        lit_appear_tmp[std::abs(l_idx)] = true;
                        if (!l->is_equal)
                        {
                            if ((delta <= 0 && l_idx > 0) || (delta > 0 && l_idx < 0))
                                sat_flag = true;
                        }
                        else if ((delta == 0 && l_idx > 0) || (delta != 0 && l_idx < 0))
                            sat_flag = true;
                    } // if the nia lit does not exist
                    else
                    {
                        if (!l->is_equal)
                        {
                            if ((l->delta <= 0 && l_idx > 0) || (l->delta > 0 && l_idx < 0))
                                sat_flag = true;
                        }
                        else if ((l->delta == 0 && l_idx > 0) || (l->delta != 0 && l_idx < 0))
                            sat_flag = true;
                    } // if the nia lit exists
                } // nia var
                else if ((_solution[l->delta.to_int()] > 0 && l_idx > 0) || (_solution[l->delta.to_int()] < 0 && l_idx < 0))
                    sat_flag = true; // boolean var
                if (sat_flag == true)
                    break;
            }
            if (sat_flag == false)
            {
                lit *l = &(_lits[std::abs(cl.literals[0])]);
                _solution[l->delta.to_int()] = 1;
            } // if the clause is false, flip the var
        }
    }

    bool ls_solver::get_lits_value(uint64_t lit_idx, const std::string &str)
    {
#ifdef NLS_DEBUG
        std::cout << str << "\n";
#endif
        if (lit_idx < _num_lits && _lits[lit_idx].lits_index != 0)
        {
            std::cout << lit_idx << " :::" << _lits[lit_idx].is_true << "\n";
            return _lits[lit_idx].is_true > 0;
        }
        else
        {
        }
        return true;
    }

    void ls_solver::record_soft_var_solution(std::vector<int> &soft_c_info)
    {
        size_t soft_info_size = soft_components_idx.size();
        soft_c_info.resize(soft_info_size);
        for (size_t i = 0; i < soft_info_size; i++)
        {
            int var_idx = soft_components_idx[i];
            soft_c_info[i] = _solution[var_idx].to_int();
#ifdef NLS_DEBUG
            std::cout << soft_c_info[i] << std::endl;
#endif
        }
    }
}
