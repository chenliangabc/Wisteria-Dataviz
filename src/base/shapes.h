/** @addtogroup Graphics
    @brief Graphing classes.
    @date 2005-2022
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef __WISTERIA_SHAPES_H__
#define __WISTERIA_SHAPES_H__

#include "graphitems.h"
#include "colorbrewer.h"

namespace Wisteria::GraphItems
    {
    /** @brief Helper class to draw shapes.
        @details This class accepts a GraphItemInfo object, which will be used
            by the @c DrawXX() functions.\n
            This class is not an embeddable object placed on a canvas like
            `GraphItemBase`-deirived classes; rather, it is used by these classes
            as a way to draw commonly used shapes.
        @note This class is used by Point2D and Label objects and not meant to be
            used by client code. Prefer using Point2D for drawing icons on a graph.*/
    class Shapes
        {
    public:
        /** @brief Constructor.
            @param itemInfo Extended information to construct this item with.*/
        explicit Shapes(const GraphItemInfo& itemInfo) : m_graphInfo(itemInfo)
            {}
        /// @brief Gets/sets the shape's underlying information (e.g., brush color, pen, etc.).
        /// @details This is useful for changing the shape's settings when preparing to
        ///     draw different shapes.
        /// @returns The shape's information.
        [[nodiscard]] GraphItemInfo& GetGraphItemInfo() noexcept
            { return m_graphInfo; }
        /// @brief Draws a circle filled with the shape's brush, draws a black
        ///     outline, and draws the shape's text value in the center of it.
        /// @param rect The area to bind the circle within.
        /// @param dc The DC to draw to.
        void DrawCircularSign(const wxRect rect, wxDC& dc);
        /// @brief Draws a yellow sun shape (circle with sunbeams).
        /// @param rect The area to bind the sun within.
        /// @param dc The DC to draw to.
        void DrawSun(const wxRect rect, wxDC& dc);
        /// @brief Draws a purple flower shape (stigma and petals).
        /// @param rect The area to bind the flower within.
        /// @param dc The DC to draw to.
        void DrawFlower(const wxRect rect, wxDC& dc);
        /// @brief Draws a ref leaf.
        /// @param rect The area to bind the leaf within.
        /// @param dc The DC to draw to.
        void DrawFallLeaf(const wxRect rect, wxDC& dc);
    private:
        /// @brief Helper to get X coordinate based on percent of width of rect from its left side.
        [[nodiscard]] double GetXPosFromLeft(const wxRect rect, const double percentFromLeft) noexcept
            {
            wxASSERT_MSG(percentFromLeft >= 0.0 && percentFromLeft <= 1.0,
                L"Percent should be between 0.0 and 1.0!");
            return rect.GetLeft() + (rect.GetWidth() * percentFromLeft);
            };
        /// @brief Helper to get Y coordinate based on percent of height of rect from its top.
        [[nodiscard]] double GetYPosFromTop(const wxRect rect, const double percentFromLeft) noexcept
            {
            wxASSERT_MSG(percentFromLeft >= 0.0 && percentFromLeft <= 1.0,
                L"Percent should be between 0.0 and 1.0!");
            return rect.GetTop() + (rect.GetHeight() * percentFromLeft);
            };
        [[nodiscard]] double GetScaling() const noexcept
            { return m_graphInfo.GetScaling(); }

        double GetDPIScaleFactor() const noexcept
            {
            wxASSERT_LEVEL_2_MSG(m_graphInfo.GetDPIScaleFactor().has_value(),
                                 L"Shape should have a proper DPI scaling.");
            return (m_graphInfo.GetDPIScaleFactor().has_value() ?
                    m_graphInfo.GetDPIScaleFactor().value() : 1);
            }

        [[nodiscard]] double ScaleToScreenAndCanvas(const double value) const noexcept
            { return value * GetScaling() * GetDPIScaleFactor(); }
        GraphItemInfo m_graphInfo;
        };
    };

/** @}*/

#endif //__WISTERIA_SHAPES_H__
