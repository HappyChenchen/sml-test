#include "matrix.h"
#include <cstdint>
#define NMDEBUG

// swap rows
void swapRows(std::vector<std::vector<ration_num>> &matrix, int i, int j)
{
    std::vector<ration_num> temp = matrix[i];
    matrix[i] = matrix[j];
    matrix[j] = temp;
}

void print_matrix(std::vector<std::vector<ration_num>> &matrix)
{
#ifdef MDEBUG
    size_t row_num = matrix.size();
    size_t collom_num = matrix[0].size();
    for (int i = 0; i < row_num; ++i)
    {
        for (int j = 0; j < collom_num; ++j)
            std::cout << matrix[i][j].print() << " ";
        std::cout << std::endl;
    }
    std::cout << std::endl;
#endif
}

// find the min non-zero var in the collom as the pivot
int findPivot(const std::vector<std::vector<ration_num>> &matrix, int col, int startRow)
{
    int minRow = startRow;
    ration_num minAbs = INT32_MAX;
    for (int i = startRow; i < matrix.size(); ++i)
    {
        if (matrix[i][col].abs() < minAbs && matrix[i][col] != 0)
        {
            minRow = i;
            minAbs = matrix[i][col].abs();
        }
    }
    return minRow;
}

void transfer_to_integer(std::vector<ration_num> &row)
{
    int64_t m_total = 1;
    for (const auto &item : row)
        m_total *= item.m;
    for (auto &item : row)
        item *= m_total;
    int64_t result = row[0].n;
    for (const auto &item : row)
    {
        result = std::gcd(result, item.n);
        if (result == 1)
            return;
    }
    if (result != 1)
    {
        for (auto &item : row)
            item /= result;
    }
}

void transfer_to_integer(std::vector<std::vector<ration_num>> &matrix)
{
    for (auto &row : matrix)
    {
        if (!is_all_zero_row(row))
            transfer_to_integer(row);
    }
}

// update the upper row by low row
void merge_rows(std::vector<ration_num> &upper_row, const std::vector<ration_num> &low_row)
{
    std::map<ration_num, int> portion2fre;
    int zero_fre = 0;
    for (size_t c_idx = 0; c_idx < low_row.size() - 1; c_idx++)
    {
        if (low_row[c_idx] != 0)
        {
            if (upper_row[c_idx] == 0)
                zero_fre++;
            else
            {
                ration_num p = upper_row[c_idx] / low_row[c_idx];
                if (portion2fre.find(p) == portion2fre.end())
                    portion2fre[p] = 1;
                else
                    portion2fre[p]++;
            }
        }
    }
    ration_num max_p = 0;
    int max_p_fre = 0;
    for (const auto &pair : portion2fre)
    {
        if (pair.second > max_p_fre)
        {
            max_p = pair.first;
            max_p_fre = pair.second;
        }
    }
    if (max_p_fre > zero_fre)
    {
        for (size_t c_idx = 0; c_idx < low_row.size(); c_idx++)
        {
            upper_row[c_idx] -= max_p * low_row[c_idx];
        }
    }
}

void gaussianElimination(std::vector<std::vector<ration_num>> &matrix)
{
    int row_num = (int)matrix.size();
    int collom_num = (int)matrix[0].size();

    // turn the matrix to upper matrix
    for (int i = 0; i < row_num && i < collom_num; ++i)
    {
        // find the pivot with min non-zero abs value, if cannot find, means the whole collom is zero
        int minRow = findPivot(matrix, i, i);
        if (matrix[minRow][i] == 0)
            continue;
        // swap row if the min non-zero abs row is not the current one
        if (minRow != i)
            swapRows(matrix, i, minRow);
        // eleminate vars
        for (int j = i + 1; j < row_num; ++j)
        {
            if (matrix[j][i] != 0)
            {
                ration_num factor = matrix[j][i] / matrix[i][i];
                for (int k = i; k < collom_num; ++k)
                    matrix[j][k] -= factor * matrix[i][k];
            }
        }
    }
    print_matrix(matrix);
    // turn the upper triangle matrix to diagonal matrix
    for (int i = row_num - 1; i >= 0; i--)
    {
        bool non_zero = false;
        int non_zero_idx = i;
        for (; non_zero_idx < collom_num - 1; non_zero_idx++)
        {
            if (matrix[i][non_zero_idx] != 0)
            {
                non_zero = true;
                break;
            }
        }
        // if the row are all zeros, continue, otherwise delete other rows by the non-zero one
        if (!non_zero)
            continue;
        for (int j = i - 1; j >= 0; j--)
        {
            ration_num factor = matrix[j][non_zero_idx] / matrix[i][non_zero_idx];
            for (int k = non_zero_idx; k < collom_num; k++)
                matrix[j][k] -= factor * matrix[i][k];
        }
    }
    print_matrix(matrix);
    for (int i = row_num - 1; i >= 0; i--)
    {
        for (int j = i - 1; j >= 0; j--)
        {
            merge_rows(matrix[j], matrix[i]);
        }
    }
    print_matrix(matrix);
    transfer_to_integer(matrix);
    print_matrix(matrix);
}

bool contains_short_row(const std::vector<std::vector<ration_num>> &matrix)
{
    size_t collom_num = matrix[0].size();
    for (size_t r_idx = 0; r_idx < matrix.size(); r_idx++)
    {
        int non_zero_cnt = 0;
        for (size_t c_idx = 0; c_idx < collom_num - 1; c_idx++)
            if (matrix[r_idx][c_idx] != 0)
                non_zero_cnt++;
        if (non_zero_cnt <= 2)
            return true;
    }
    return false;
}

bool is_all_zero_row(const std::vector<ration_num> &row)
{
    for (const auto &item : row)
        if (item != 0)
            return false;
    return true;
}
