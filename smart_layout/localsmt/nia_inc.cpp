//
//  nia_inc.cpp
//  LS_NIA
//
//  Created by DouglasLee on 2023/12/6.
//

#include "nia_ls.h"
#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <fstream>
namespace nia_overall
{

    bool ls_solver::update_preset_value(uint64_t var_idx, ration_num pre_set_val)
    {
        ration_num val_value = _solution[var_idx];
        variable *v = &_vars[var_idx];
        if (pre_set_val <= v->upper_bound && pre_set_val >= v->low_bound)
        {
            if (v->clause_idxs.size() > 0)
                critical_move(var_idx, pre_set_val - val_value);
            else
                _solution[var_idx] = pre_set_val;
            if (unchanged_var_1 == UINT32_MAX)
                unchanged_var_1 = var_idx;
            else if (unchanged_var_2 == UINT32_MAX)
                unchanged_var_2 = var_idx;
            return true;
        }
        return false;
    }

    bool ls_solver::update_width_hight(ration_num new_pre_val_width, ration_num new_pre_val_hight)
    {
        unchanged_var_1 = UINT32_MAX;
        unchanged_var_2 = UINT32_MAX;
        new_pre_val_width = (new_pre_val_width - fa_const[b_width_idx]) / fa_coff[b_width_idx];
        new_pre_val_hight = (new_pre_val_hight - fa_const[b_hight_idx]) / fa_coff[b_hight_idx];
        return (update_preset_value(fa[b_hight_idx], new_pre_val_hight) &&
                update_preset_value(fa[b_width_idx], new_pre_val_width));
    }
}
