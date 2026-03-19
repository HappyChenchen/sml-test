#pragma once
#include <iostream>
#include <vector>
#include "ration_num.h"
#include <map>
#define NMDEBUG

// swap rows
void swapRows(std::vector<std::vector<ration_num>> &matrix, int i, int j);

void print_matrix(std::vector<std::vector<ration_num>> &matrix);

// find the min non-zero var in the collom as the pivot
int findPivot(const std::vector<std::vector<ration_num>> &matrix, int col, int startRow);

void transfer_to_integer(std::vector<ration_num> &row);

void transfer_to_integer(std::vector<std::vector<ration_num>> &matrix);

// update the upper row by low row
void merge_rows(std::vector<ration_num> &upper_row, const std::vector<ration_num> &low_row);

void gaussianElimination(std::vector<std::vector<ration_num>> &matrix);

bool contains_short_row(const std::vector<std::vector<ration_num>> &matrix);

bool is_all_zero_row(const std::vector<ration_num> &row);
