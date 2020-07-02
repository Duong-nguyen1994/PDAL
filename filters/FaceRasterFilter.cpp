/******************************************************************************
 * Copyright (c) 2020, Hobu Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following
 * conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Hobu, Inc. or Flaxen Geo Consulting nor the
 *       names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 ****************************************************************************/

#include "FaceRasterFilter.hpp"

#include <pdal/util/Utils.hpp>
#include <pdal/private/MathUtils.hpp>

namespace pdal
{

static StaticPluginInfo const s_info
{
    "filters.faceraster",
    "Face Raster Filter",
    "http://pdal.io/stages/filters.faceraster.html"
};

CREATE_STATIC_STAGE(FaceRasterFilter, s_info)

std::string FaceRasterFilter::getName() const
{
    return s_info.name;
}


FaceRasterFilter::FaceRasterFilter()
{}


void FaceRasterFilter::addArgs(ProgramArgs& args)
{
    m_limits.addArgs(args);
    args.add("mesh", "Mesh name", m_meshName);
}

void FaceRasterFilter::prepared(PointTableRef)
{
    int cnt = m_limits.checkArgs();
    if (cnt != 0 && cnt != 4)
        throwError("Must specify all or none of 'origin_x', 'origin_y', 'width' and 'height'.");
    m_computeLimits = (cnt == 0);
}


void FaceRasterFilter::filter(PointView& v)
{
    // If the user hasn't set bounds, set them based on the data.
    if (!m_computeLimits)
    {
        BOX2D bounds;
        v.calculateBounds(bounds);
        m_limits.xOrigin = bounds.minx - (m_limits.edgeLength / 2);
        m_limits.yOrigin = bounds.miny - (m_limits.edgeLength / 2);
        m_limits.width = ((bounds.maxx - m_limits.xOrigin) / m_limits.edgeLength) + 1;
        m_limits.height = ((bounds.maxy - m_limits.yOrigin) / m_limits.edgeLength) + 1;
    }
    Rasterd *raster = v.createRaster("faceraster", m_limits);
    if (!raster)
        throwError("Raster already exists");

    TriangularMesh *m = v.mesh(m_meshName);
    if (!m)
        throwError("Mesh '" + m_meshName + "; does not exist.");

    for (const Triangle& t : *m)
    {
        double x1 = v.getFieldAs<double>(Dimension::Id::X, t.m_a) - m_limits.xOrigin;
        double y1 = v.getFieldAs<double>(Dimension::Id::Y, t.m_a) - m_limits.yOrigin;
        double z1 = v.getFieldAs<double>(Dimension::Id::Z, t.m_a);

        double x2 = v.getFieldAs<double>(Dimension::Id::X, t.m_b) - m_limits.xOrigin;
        double y2 = v.getFieldAs<double>(Dimension::Id::Y, t.m_b) - m_limits.yOrigin;
        double z2 = v.getFieldAs<double>(Dimension::Id::Z, t.m_b);

        double x3 = v.getFieldAs<double>(Dimension::Id::X, t.m_c) - m_limits.xOrigin;
        double y3 = v.getFieldAs<double>(Dimension::Id::Y, t.m_c) - m_limits.yOrigin;
        double z3 = v.getFieldAs<double>(Dimension::Id::Z, t.m_c);

        double xmax = (std::max)((std::max)(x1, x2), x3);
        double xmin = (std::min)((std::min)(x1, x2), x3);
        double ymax = (std::max)((std::max)(y1, y2), y3);
        double ymin = (std::min)((std::min)(y1, y2), y3);

        double halfEdge = m_limits.edgeLength / 2;
        int ax = raster->horizontalIndex(xmin + halfEdge);
        int ay = raster->verticalIndex(ymin + halfEdge);

        int bx = raster->horizontalIndex(xmax + halfEdge);
        int by = raster->verticalIndex(ymax + halfEdge);

        ax = Utils::clamp(ax, 0, (int)m_limits.width);
        bx = Utils::clamp(bx, 0, (int)m_limits.width);
        ay = Utils::clamp(ay, 0, (int)m_limits.height);
        by = Utils::clamp(by, 0, (int)m_limits.height);

        for (size_t xi = ax; (int)xi < bx; ++xi)
            for (size_t yi = ay; (int)yi < by; ++yi)
            {
                double x = raster->horizontalPos(xi);
                double y = raster->verticalPos(yi);

                double val = math::barycentricInterpolation(x1, y1, z1,
                    x2, y2, z2, x3, y3, z3, x, y);
                if (val != std::numeric_limits<double>::infinity())
                    raster->at(xi, yi) = val;
            }
    }
}

} // namespace pdal