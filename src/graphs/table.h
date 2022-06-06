/** @addtogroup Graphics
    @brief Graphing classes.
    @date 2005-2022
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef __WISTERIA_TABLE_H__
#define __WISTERIA_TABLE_H__

#include <vector>
#include <variant>
#include "graph2d.h"

namespace Wisteria::Graphs
    {
    class Table final : public Graph2D
        {
    public:
        /// @brief Types of values that can be used for a cell.
        using CellValueType = std::variant<double, wxString, wxDateTime>;

        /// @brief A cell in the table.
        class TableCell
            {
            friend class Table;
        public:
            /// @brief Constructor.
            /// @param value The value for the cell.
            /// @param bgColor The cell's background color.
            TableCell(const CellValueType& value, const wxColour bgColor) :
                m_value(value), m_bgColor(bgColor)
                {}
            /// @private
            TableCell() = default;
            /// @brief Gets the value as it is displayed in the cell.
            /// @returns The displayable string for the cell.
            [[nodiscard]] wxString GetDisplayValue() const
                {
                if (const auto strVal{ std::get_if<wxString>(&m_value) }; strVal)
                    { return *strVal; }
                else if (const auto dVal{ std::get_if<double>(&m_value) }; dVal)
                    {
                    if (std::isnan(*dVal))
                        { return wxEmptyString; }
                    return wxNumberFormatter::ToString(*dVal, 0,
                        wxNumberFormatter::Style::Style_WithThousandsSep);
                    }
                else if (const auto dVal{ std::get_if<wxDateTime>(&m_value) }; dVal)
                    {
                    if (!dVal->IsValid())
                        { return wxEmptyString; }
                    return dVal->FormatDate();
                    }
                else
                    { return wxEmptyString; }
                }

            /// @returns @c true if the cell is text.
            [[nodiscard]] bool IsText() const noexcept
                { return (std::get_if<wxString>(&m_value) != nullptr); }
            /// @returns @c true if the cell is a number.
            [[nodiscard]] bool IsNumeric() const noexcept
                { return (std::get_if<double>(&m_value) != nullptr); }
            /// @returns @c true if the cell is a date.
            [[nodiscard]] bool IsDate() const noexcept
                { return (std::get_if<wxDateTime>(&m_value) != nullptr); }

            /// @brief Sets the value.
            /// @param value The value to set for the cell.
            void SetValue(const CellValueType& value)
                { m_value = value; }
            /// @brief Sets the color.
            /// @param color The value to set for the cell.
            void SetBackgroundColor(const wxColour color)
                { m_bgColor = color; }
            /// @brief Sets the number of columns that this cell should consume.
            /// @param colCount The number of cells that this should consume horizontally.
            void SetColumnCount(const int colCount) noexcept
                {
                if (colCount <= 0)
                    { m_columnCount = 1; }
                else
                    { m_columnCount = colCount; }
                }
            /// @brief Sets the number of rows that this cell should consume.
            /// @param rowCount The number of cells that this should consume vertically.
            void SetRowCount(const int rowCount) noexcept
                {
                if (rowCount <= 0)
                    { m_rowCount = 1; }
                else
                    { m_rowCount = rowCount; }
                }
        private:
            CellValueType m_value{ std::numeric_limits<double>::quiet_NaN() };
            wxColour m_bgColor{ *wxWHITE };
            int m_columnCount{ 1 };
            int m_rowCount{ 1 };
            };

        /// @brief Constructor.
        /// @param canvas The canvas to draw the table on.
        explicit Table(Wisteria::Canvas* canvas);

        /** @brief Set the display across the heatmap.
            @param data The data.
            @param columns The columns to display in the table.\n
                The columns will appear in the order that you specify here.
            @param transpose @c true to transpose the data (i.e., display the columns
                from the data as rows).
            @throws std::runtime_error If any columns can't be found by name,
             throws an exception.\n
             The exception's @c what() message is UTF-8 encoded, so pass it to
             @c wxString::FromUTF8() when formatting it for an error message.*/
        void SetData(const std::shared_ptr<const Data::Dataset>& data,
                     const std::initializer_list<wxString>& columns,
                     const bool transpose = false);
        /// @brief Inserts an empty row at the given index.
        /// @details For example, an index of @c 0 will insert the row at the
        ///     top of the table.
        /// @note If the table's size has not been established yet (via SetData()
        ///     or SetTableSize()), then calls to this will ignored since the
        ///     number of columns is unknown.
        /// @param rowIndex Where to insert the row.
        void InsertRow(const size_t rowIndex)
            {
            if (m_table.size())
                {
                m_table.insert(m_table.cbegin() +
                        // clamp indices going beyond the row count to m_table.cend()
                        std::clamp<size_t>(rowIndex, 0, m_table.size()),
                    std::vector<TableCell>(m_table[0].size(), TableCell()));
                }
            }
        /// @brief Inserts an empty column at the given index.
        /// @details For example, an index of @c 0 will insert the column at the
        ///     left side of the table.
        /// @note If the table's size has not been established yet (via SetData()
        ///     or SetTableSize()), then calls to this will ignored since there will
        ///     be no rows to insert columns into.
        /// @param colIndex Where to insert the column.
        void InsertColumn(const size_t colIndex)
            {
            if (m_table.size())
                {
                for (auto& row : m_table)
                    {
                    row.insert(row.cbegin() +
                        // clamp indices going beyond the column count to row.cend()
                        std::clamp<size_t>(colIndex, 0, row.size()),
                        TableCell());
                    }
                }
            }
        /// @brief Sets the size of the table.
        /// @details This should only be used if building a table from scratch.
        ///     Prefer using SetData() instead.
        /// @param rows The number of rows.
        /// @param cols The number of columns.
        /// @note If the table is being made smaller, then existing content
        ///     outside of the new size will be removed; other existing content
        ///     will be preserved.\n
        ///     Call ClearTable() to clear any existing content if you wish to
        ///     reset the table.
        void SetTableSize(const size_t rows, const size_t cols)
            {
            m_table.resize(rows);
            for (auto& row : m_table)
                { row.resize(cols); }
            }
        /// @brief Empties the contents of the table.
        void ClearTable()
            { m_table.clear(); }
        /// @brief Accesses the cell at a given position.
        /// @param row The row index of the cell.
        /// @param column The column index of the cell.
        /// @throws std::runtime_error If row or column index is out of range, throws an exception.\n
        ///     The exception's @c what() message is UTF-8 encoded, so pass it to
        ///     @c wxString::FromUTF8() when formatting it for an error message.
        TableCell& GetCell(const size_t row, const size_t column)
            {
            wxASSERT_MSG(row < m_table.size(),
                wxString::Format(L"Invalid row index (%zu)!", row));
            wxASSERT_MSG(column < m_table[row].size(),
                wxString::Format(L"Invalid column index (%zu)!", column));
            if (row >= m_table.size() || column >= m_table[row].size())
                {
                throw std::runtime_error(
                    wxString::Format(_(L"Invalid cell index (row %zu, column %zu)."),
                                     row, column).ToUTF8());
                }
            return m_table[row][column];
            }
        /// @brief Sets the minimum percent of the drawing area's width that the
        ///     table should consume (between 0.0 to 1.0, representing 0% to 100%).
        /// @param percent The minimum percent of the area's width that the table should consume.
        void SetMinWidthProportion(const double percent)
            { m_minWidthProportion = std::clamp(percent, 0.0, 1.0); }
        /// @brief Sets the minimum percent of the drawing area's height that the
        ///     table should consume (between 0.0 to 1.0, representing 0% to 100%).
        /// @param percent The minimum percent of the area's height that the table should consume.
        void SetMinHeightProportion(const double percent)
            { m_minHeightProportion = std::clamp(percent, 0.0, 1.0); }
    private:
        void RecalcSizes(wxDC& dc) final;
        std::vector<std::vector<TableCell>> m_table;
        std::optional<double> m_minWidthProportion;
        std::optional<double> m_minHeightProportion;
        };
    }

/** @}*/

#endif //__WISTERIA_TABLE_H__
