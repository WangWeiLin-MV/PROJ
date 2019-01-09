/******************************************************************************
 *
 * Project:  PROJ
 * Purpose:  Test ISO19111:2018 implementation
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gtest_include.h"

// to be able to use internal::toString
#define FROM_PROJ_CPP

#include "proj/common.hpp"
#include "proj/coordinateoperation.hpp"
#include "proj/coordinatesystem.hpp"
#include "proj/crs.hpp"
#include "proj/datum.hpp"
#include "proj/io.hpp"
#include "proj/metadata.hpp"
#include "proj/util.hpp"

#include "proj/internal/internal.hpp"

#include "proj_constants.h"

#include <string>

using namespace osgeo::proj::common;
using namespace osgeo::proj::crs;
using namespace osgeo::proj::cs;
using namespace osgeo::proj::datum;
using namespace osgeo::proj::internal;
using namespace osgeo::proj::io;
using namespace osgeo::proj::metadata;
using namespace osgeo::proj::operation;
using namespace osgeo::proj::util;

// ---------------------------------------------------------------------------

TEST(io, wkt_parsing) {
    {
        auto n = WKTNode::createFrom("MYNODE[]");
        EXPECT_EQ(n->value(), "MYNODE");
        EXPECT_TRUE(n->children().empty());
        EXPECT_EQ(n->toString(), "MYNODE");
    }
    {
        auto n = WKTNode::createFrom("  MYNODE  [  ]  ");
        EXPECT_EQ(n->value(), "MYNODE");
        EXPECT_TRUE(n->children().empty());
    }
    EXPECT_THROW(WKTNode::createFrom(""), ParsingException);
    EXPECT_THROW(WKTNode::createFrom("x"), ParsingException);
    EXPECT_THROW(WKTNode::createFrom("x,"), ParsingException);
    EXPECT_THROW(WKTNode::createFrom("x["), ParsingException);
    EXPECT_THROW(WKTNode::createFrom("["), ParsingException);

    {
        auto n = WKTNode::createFrom("MYNODE[\"x\"]");
        EXPECT_EQ(n->value(), "MYNODE");
        ASSERT_EQ(n->children().size(), 1);
        EXPECT_EQ(n->children()[0]->value(), "\"x\"");
        EXPECT_EQ(n->toString(), "MYNODE[\"x\"]");
    }

    EXPECT_THROW(WKTNode::createFrom("MYNODE[\"x\""), ParsingException);

    {
        auto n = WKTNode::createFrom("MYNODE[  \"x\"   ]");
        EXPECT_EQ(n->value(), "MYNODE");
        ASSERT_EQ(n->children().size(), 1);
        EXPECT_EQ(n->children()[0]->value(), "\"x\"");
    }

    {
        auto n = WKTNode::createFrom("MYNODE[\"x[\",1]");
        EXPECT_EQ(n->value(), "MYNODE");
        ASSERT_EQ(n->children().size(), 2);
        EXPECT_EQ(n->children()[0]->value(), "\"x[\"");
        EXPECT_EQ(n->children()[1]->value(), "1");
        EXPECT_EQ(n->toString(), "MYNODE[\"x[\",1]");
    }

    EXPECT_THROW(WKTNode::createFrom("MYNODE[\"x\","), ParsingException);

    {
        auto n = WKTNode::createFrom("A[B[y]]");
        EXPECT_EQ(n->value(), "A");
        ASSERT_EQ(n->children().size(), 1);
        EXPECT_EQ(n->children()[0]->value(), "B");
        ASSERT_EQ(n->children()[0]->children().size(), 1);
        EXPECT_EQ(n->children()[0]->children()[0]->value(), "y");
        EXPECT_EQ(n->toString(), "A[B[y]]");
    }

    EXPECT_THROW(WKTNode::createFrom("A[B["), ParsingException);

    std::string str;
    for (int i = 0; i < 17; i++) {
        str = "A[" + str + "]";
    }
    EXPECT_THROW(WKTNode::createFrom(str), ParsingException);

    {
        auto wkt = "A[\"a\",B[\"b\",C[\"c\"]],D[\"d\"]]";
        EXPECT_EQ(WKTNode::createFrom(wkt)->toString(), wkt);
    }
}

// ---------------------------------------------------------------------------

TEST(io, wkt_parsing_with_parenthesis) {

    auto n = WKTNode::createFrom("A(\"x\",B(\"y\"))");
    EXPECT_EQ(n->toString(), "A[\"x\",B[\"y\"]]");
}

// ---------------------------------------------------------------------------

TEST(io, wkt_parsing_with_double_quotes_inside) {

    auto n = WKTNode::createFrom("A[\"xy\"\"z\"]");
    EXPECT_EQ(n->children()[0]->value(), "\"xy\"z\"");
    EXPECT_EQ(n->toString(), "A[\"xy\"\"z\"]");

    EXPECT_THROW(WKTNode::createFrom("A[\"x\""), ParsingException);
}

// ---------------------------------------------------------------------------

TEST(io, wkt_parsing_with_printed_quotes) {
    static const std::string startPrintedQuote("\xE2\x80\x9C");
    static const std::string endPrintedQuote("\xE2\x80\x9D");

    auto n = WKTNode::createFrom("A[" + startPrintedQuote + "x" +
                                 endPrintedQuote + "]");
    EXPECT_EQ(n->children()[0]->value(), "\"x\"");
    EXPECT_EQ(n->toString(), "A[\"x\"]");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, sphere) {
    auto obj = WKTParser().createFromWKT(
        "ELLIPSOID[\"Sphere\",6378137,0,LENGTHUNIT[\"metre\",1]]");
    auto ellipsoid = nn_dynamic_pointer_cast<Ellipsoid>(obj);
    ASSERT_TRUE(ellipsoid != nullptr);
    EXPECT_TRUE(ellipsoid->isSphere());
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, datum_with_ANCHOR) {
    auto obj = WKTParser().createFromWKT(
        "DATUM[\"WGS_1984 with anchor\",\n"
        "    ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "        LENGTHUNIT[\"metre\",1,\n"
        "            ID[\"EPSG\",9001]],\n"
        "        ID[\"EPSG\",7030]],\n"
        "    ANCHOR[\"My anchor\"]]");
    auto datum = nn_dynamic_pointer_cast<GeodeticReferenceFrame>(obj);
    ASSERT_TRUE(datum != nullptr);
    EXPECT_EQ(datum->ellipsoid()->celestialBody(), "Earth");
    EXPECT_EQ(datum->primeMeridian()->nameStr(), "Greenwich");
    auto anchor = datum->anchorDefinition();
    EXPECT_TRUE(anchor.has_value());
    EXPECT_EQ(*anchor, "My anchor");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, datum_no_pm_not_earth) {
    auto obj = WKTParser().createFromWKT("DATUM[\"unnamed\",\n"
                                         "    ELLIPSOID[\"unnamed\",1,0,\n"
                                         "        LENGTHUNIT[\"metre\",1]]]");
    auto datum = nn_dynamic_pointer_cast<GeodeticReferenceFrame>(obj);
    ASSERT_TRUE(datum != nullptr);
    EXPECT_EQ(datum->ellipsoid()->celestialBody(), "Non-Earth body");
    EXPECT_EQ(datum->primeMeridian()->nameStr(), "Reference meridian");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, dynamic_geodetic_reference_frame) {
    auto obj = WKTParser().createFromWKT(
        "GEOGCRS[\"WGS 84 (G1762)\","
        "DYNAMIC[FRAMEEPOCH[2005.0]],"
        "TRF[\"World Geodetic System 1984 (G1762)\","
        "    ELLIPSOID[\"WGS 84\",6378137,298.257223563],"
        "    ANCHOR[\"My anchor\"]],"
        "CS[ellipsoidal,3],"
        "    AXIS[\"(lat)\",north,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"(lon)\",east,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"ellipsoidal height (h)\",up,LENGTHUNIT[\"metre\",1.0]]"
        "]");
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    auto dgrf =
        std::dynamic_pointer_cast<DynamicGeodeticReferenceFrame>(crs->datum());
    ASSERT_TRUE(dgrf != nullptr);
    auto anchor = dgrf->anchorDefinition();
    EXPECT_TRUE(anchor.has_value());
    EXPECT_EQ(*anchor, "My anchor");
    EXPECT_TRUE(dgrf->frameReferenceEpoch() ==
                Measure(2005.0, UnitOfMeasure::YEAR));
    auto model = dgrf->deformationModelName();
    EXPECT_TRUE(!model.has_value());
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, dynamic_geodetic_reference_frame_with_model) {
    auto obj = WKTParser().createFromWKT(
        "GEOGCRS[\"WGS 84 (G1762)\","
        "DYNAMIC[FRAMEEPOCH[2005.0],MODEL[\"my_model\"]],"
        "TRF[\"World Geodetic System 1984 (G1762)\","
        "    ELLIPSOID[\"WGS 84\",6378137,298.257223563],"
        "    ANCHOR[\"My anchor\"]],"
        "CS[ellipsoidal,3],"
        "    AXIS[\"(lat)\",north,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"(lon)\",east,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"ellipsoidal height (h)\",up,LENGTHUNIT[\"metre\",1.0]]"
        "]");
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    auto dgrf =
        std::dynamic_pointer_cast<DynamicGeodeticReferenceFrame>(crs->datum());
    ASSERT_TRUE(dgrf != nullptr);
    auto anchor = dgrf->anchorDefinition();
    EXPECT_TRUE(anchor.has_value());
    EXPECT_EQ(*anchor, "My anchor");
    EXPECT_TRUE(dgrf->frameReferenceEpoch() ==
                Measure(2005.0, UnitOfMeasure::YEAR));
    auto model = dgrf->deformationModelName();
    EXPECT_TRUE(model.has_value());
    EXPECT_EQ(*model, "my_model");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, dynamic_geodetic_reference_frame_with_velocitygrid) {
    auto obj = WKTParser().createFromWKT(
        "GEOGCRS[\"WGS 84 (G1762)\","
        "DYNAMIC[FRAMEEPOCH[2005.0],VELOCITYGRID[\"my_model\"]],"
        "TRF[\"World Geodetic System 1984 (G1762)\","
        "    ELLIPSOID[\"WGS 84\",6378137,298.257223563],"
        "    ANCHOR[\"My anchor\"]],"
        "CS[ellipsoidal,3],"
        "    AXIS[\"(lat)\",north,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"(lon)\",east,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"ellipsoidal height (h)\",up,LENGTHUNIT[\"metre\",1.0]]"
        "]");
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    auto dgrf =
        std::dynamic_pointer_cast<DynamicGeodeticReferenceFrame>(crs->datum());
    ASSERT_TRUE(dgrf != nullptr);
    auto anchor = dgrf->anchorDefinition();
    EXPECT_TRUE(anchor.has_value());
    EXPECT_EQ(*anchor, "My anchor");
    EXPECT_TRUE(dgrf->frameReferenceEpoch() ==
                Measure(2005.0, UnitOfMeasure::YEAR));
    auto model = dgrf->deformationModelName();
    EXPECT_TRUE(model.has_value());
    EXPECT_EQ(*model, "my_model");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, geogcrs_with_ensemble) {
    auto obj = WKTParser().createFromWKT(
        "GEOGCRS[\"WGS 84\","
        "ENSEMBLE[\"WGS 84 ensemble\","
        "    MEMBER[\"WGS 84 (TRANSIT)\"],"
        "    MEMBER[\"WGS 84 (G730)\"],"
        "    MEMBER[\"WGS 84 (G834)\"],"
        "    MEMBER[\"WGS 84 (G1150)\"],"
        "    MEMBER[\"WGS 84 (G1674)\"],"
        "    MEMBER[\"WGS 84 (G1762)\"],"
        "    ELLIPSOID[\"WGS "
        "84\",6378137,298.2572236,LENGTHUNIT[\"metre\",1.0]],"
        "    ENSEMBLEACCURACY[2]"
        "],"
        "CS[ellipsoidal,3],"
        "    AXIS[\"(lat)\",north,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"(lon)\",east,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"ellipsoidal height (h)\",up,LENGTHUNIT[\"metre\",1.0]]"
        "]");
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    ASSERT_TRUE(crs->datum() == nullptr);
    ASSERT_TRUE(crs->datumEnsemble() != nullptr);
    EXPECT_EQ(crs->datumEnsemble()->datums().size(), 6);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_geogcrs_with_ensemble) {
    auto wkt =
        "GEOGCRS[\"WGS 84\","
        "ENSEMBLE[\"WGS 84 ensemble\","
        "    MEMBER[\"WGS 84 (TRANSIT)\"],"
        "    MEMBER[\"WGS 84 (G730)\"],"
        "    MEMBER[\"WGS 84 (G834)\"],"
        "    MEMBER[\"WGS 84 (G1150)\"],"
        "    MEMBER[\"WGS 84 (G1674)\"],"
        "    MEMBER[\"WGS 84 (G1762)\"],"
        "    ENSEMBLEACCURACY[2]"
        "],"
        "CS[ellipsoidal,3],"
        "    AXIS[\"(lat)\",north,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"(lon)\",east,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"ellipsoidal height (h)\",up,LENGTHUNIT[\"metre\",1.0]]"
        "]";
    EXPECT_THROW(WKTParser().createFromWKT(wkt), ParsingException);
}

// ---------------------------------------------------------------------------

static void checkEPSG_4326(GeographicCRSPtr crs, bool latLong = true,
                           bool checkEPSGCodes = true) {
    if (checkEPSGCodes) {
        ASSERT_EQ(crs->identifiers().size(), 1);
        EXPECT_EQ(crs->identifiers()[0]->code(), "4326");
        EXPECT_EQ(*(crs->identifiers()[0]->codeSpace()), "EPSG");
    }
    EXPECT_EQ(crs->nameStr(), "WGS 84");

    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 2);
    if (latLong) {
        EXPECT_TRUE(cs->axisList()[0]->nameStr() == "Latitude" ||
                    cs->axisList()[0]->nameStr() == "Geodetic latitude")
            << cs->axisList()[0]->nameStr();
        EXPECT_EQ(tolower(cs->axisList()[0]->abbreviation()), "lat");
        EXPECT_EQ(cs->axisList()[0]->direction(), AxisDirection::NORTH);

        EXPECT_TRUE(cs->axisList()[1]->nameStr() == "Longitude" ||
                    cs->axisList()[1]->nameStr() == "Geodetic longitude")
            << cs->axisList()[1]->nameStr();
        EXPECT_EQ(tolower(cs->axisList()[1]->abbreviation()), "lon");
        EXPECT_EQ(cs->axisList()[1]->direction(), AxisDirection::EAST);
    } else {
        EXPECT_EQ(cs->axisList()[0]->nameStr(), "Longitude");
        EXPECT_EQ(cs->axisList()[0]->abbreviation(), "lon");
        EXPECT_EQ(cs->axisList()[0]->direction(), AxisDirection::EAST);

        EXPECT_EQ(cs->axisList()[1]->nameStr(), "Latitude");
        EXPECT_EQ(cs->axisList()[1]->abbreviation(), "lat");
        EXPECT_EQ(cs->axisList()[1]->direction(), AxisDirection::NORTH);
    }

    auto datum = crs->datum();
    if (checkEPSGCodes) {
        ASSERT_EQ(datum->identifiers().size(), 1);
        EXPECT_EQ(datum->identifiers()[0]->code(), "6326");
        EXPECT_EQ(*(datum->identifiers()[0]->codeSpace()), "EPSG");
    }
    EXPECT_EQ(datum->nameStr(), "World Geodetic System 1984");

    auto ellipsoid = datum->ellipsoid();
    EXPECT_EQ(ellipsoid->semiMajorAxis().value(), 6378137.0);
    EXPECT_EQ(ellipsoid->semiMajorAxis().unit(), UnitOfMeasure::METRE);
    EXPECT_EQ(ellipsoid->inverseFlattening()->value(), 298.257223563);
    if (checkEPSGCodes) {
        ASSERT_EQ(ellipsoid->identifiers().size(), 1);
        EXPECT_EQ(ellipsoid->identifiers()[0]->code(), "7030");
        EXPECT_EQ(*(ellipsoid->identifiers()[0]->codeSpace()), "EPSG");
    }
    EXPECT_EQ(ellipsoid->nameStr(), "WGS 84");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_EPSG_4326) {
    auto obj = WKTParser().createFromWKT(
        "GEOGCS[\"WGS 84\","
        "    DATUM[\"WGS_1984\","
        "        SPHEROID[\"WGS 84\",6378137,298.257223563,"
        "            AUTHORITY[\"EPSG\",\"7030\"]],"
        "        AUTHORITY[\"EPSG\",\"6326\"]],"
        "    PRIMEM[\"Greenwich\",0,"
        "        AUTHORITY[\"EPSG\",\"8901\"]],"
        "    UNIT[\"degree\",0.0174532925199433,"
        "        AUTHORITY[\"EPSG\",\"9122\"]],"
        "    AUTHORITY[\"EPSG\",\"4326\"]]");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkEPSG_4326(crs, false /* longlat order */);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_EPSG_4267) {
    auto obj =
        WKTParser()
            .attachDatabaseContext(DatabaseContext::create())
            .createFromWKT(
                "GEOGCS[\"NAD27\","
                "    DATUM[\"North_American_Datum_1927\","
                "        SPHEROID[\"Clarke 1866\",6378206.4,294.978698213898,"
                "            AUTHORITY[\"EPSG\",\"7008\"]],"
                "        AUTHORITY[\"EPSG\",\"6267\"]],"
                "    PRIMEM[\"Greenwich\",0,"
                "        AUTHORITY[\"EPSG\",\"8901\"]],"
                "    UNIT[\"degree\",0.0174532925199433,"
                "        AUTHORITY[\"EPSG\",\"9122\"]],"
                "    AUTHORITY[\"EPSG\",\"4267\"]]");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    auto datum = crs->datum();
    ASSERT_EQ(datum->identifiers().size(), 1);
    EXPECT_EQ(datum->identifiers()[0]->code(), "6267");
    EXPECT_EQ(*(datum->identifiers()[0]->codeSpace()), "EPSG");
    EXPECT_EQ(datum->nameStr(), "North American Datum 1927");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_EPSG_4807_grad_mess) {
    auto obj = WKTParser().createFromWKT(
        "GEOGCS[\"NTF (Paris)\",\n"
        "    DATUM[\"Nouvelle_Triangulation_Francaise_Paris\",\n"
        "        SPHEROID[\"Clarke 1880 (IGN)\",6378249.2,293.466021293627,\n"
        "            AUTHORITY[\"EPSG\",\"6807\"]],\n"
        "        AUTHORITY[\"EPSG\",\"6807\"]],\n"
        /* WKT1_GDAL weirdness: PRIMEM is converted to degree */
        "    PRIMEM[\"Paris\",2.33722917,\n"
        "        AUTHORITY[\"EPSG\",\"8903\"]],\n"
        "    UNIT[\"grad\",0.015707963267949,\n"
        "        AUTHORITY[\"EPSG\",\"9105\"]],\n"
        "    AXIS[\"latitude\",NORTH],\n"
        "    AXIS[\"longitude\",EAST],\n"
        "    AUTHORITY[\"EPSG\",\"4807\"]]");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    auto datum = crs->datum();
    auto primem = datum->primeMeridian();
    EXPECT_EQ(primem->longitude().unit(), UnitOfMeasure::GRAD);
    // Check that we have corrected the value that was in degree into grad.
    EXPECT_EQ(primem->longitude().value(), 2.5969213);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_geographic_old_datum_name_from_EPSG_code) {
    auto wkt =
        "GEOGCS[\"S-JTSK (Ferro)\",\n"
        "    "
        "DATUM[\"System_Jednotne_Trigonometricke_Site_Katastralni_Ferro\",\n"
        "        SPHEROID[\"Bessel 1841\",6377397.155,299.1528128,\n"
        "            AUTHORITY[\"EPSG\",\"7004\"]],\n"
        "        AUTHORITY[\"EPSG\",\"6818\"]],\n"
        "    PRIMEM[\"Ferro\",-17.66666666666667,\n"
        "       AUTHORITY[\"EPSG\",\"8909\"]],\n"
        "    UNIT[\"degree\",0.0174532925199433,\n"
        "        AUTHORITY[\"EPSG\",\"9122\"]],\n"
        "    AUTHORITY[\"EPSG\",\"4818\"]]";
    auto obj = WKTParser()
                   .attachDatabaseContext(DatabaseContext::create())
                   .createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    auto datum = crs->datum();
    EXPECT_EQ(
        datum->nameStr(),
        "System of the Unified Trigonometrical Cadastral Network (Ferro)");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_geographic_old_datum_name_witout_EPSG_code) {
    auto wkt =
        "GEOGCS[\"S-JTSK (Ferro)\",\n"
        "    "
        "DATUM[\"System_Jednotne_Trigonometricke_Site_Katastralni_Ferro\",\n"
        "        SPHEROID[\"Bessel 1841\",6377397.155,299.1528128,\n"
        "            AUTHORITY[\"EPSG\",\"7004\"]]],\n"
        "    PRIMEM[\"Ferro\",-17.66666666666667,\n"
        "       AUTHORITY[\"EPSG\",\"8909\"]],\n"
        "    UNIT[\"degree\",0.0174532925199433,\n"
        "        AUTHORITY[\"EPSG\",\"9122\"]],\n"
        "    AUTHORITY[\"EPSG\",\"4818\"]]";
    auto obj = WKTParser()
                   .attachDatabaseContext(DatabaseContext::create())
                   .createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    auto datum = crs->datum();
    EXPECT_EQ(
        datum->nameStr(),
        "System of the Unified Trigonometrical Cadastral Network (Ferro)");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_geographic_deprecated) {
    auto wkt = "GEOGCS[\"SAD69 (deprecated)\",\n"
               "    DATUM[\"South_American_Datum_1969\",\n"
               "        SPHEROID[\"GRS 1967\",6378160,298.247167427,\n"
               "            AUTHORITY[\"EPSG\",\"7036\"]],\n"
               "        AUTHORITY[\"EPSG\",\"6291\"]],\n"
               "    PRIMEM[\"Greenwich\",0,\n"
               "        AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "    UNIT[\"degree\",0.0174532925199433,\n"
               "        AUTHORITY[\"EPSG\",\"9108\"]],\n"
               "    AUTHORITY[\"EPSG\",\"4291\"]]";
    auto obj = WKTParser()
                   .attachDatabaseContext(DatabaseContext::create())
                   .createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "SAD69");
    EXPECT_TRUE(crs->isDeprecated());
}

// ---------------------------------------------------------------------------

static std::string contentWKT2_EPSG_4326(
    "[\"WGS 84\",\n"
    "    DATUM[\"World Geodetic System 1984\",\n"
    "        ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
    "            LENGTHUNIT[\"metre\",1,\n"
    "                ID[\"EPSG\",9001]],\n"
    "            ID[\"EPSG\",7030]],\n"
    "        ID[\"EPSG\",6326]],\n"
    "    PRIMEM[\"Greenwich\",0,\n"
    "        ANGLEUNIT[\"degree\",0.0174532925199433,\n"
    "            ID[\"EPSG\",9122]],\n"
    "        ID[\"EPSG\",8901]],\n"
    "    CS[ellipsoidal,2],\n"
    "        AXIS[\"latitude\",north,\n"
    "            ORDER[1],\n"
    "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
    "                ID[\"EPSG\",9122]]],\n"
    "        AXIS[\"longitude\",east,\n"
    "            ORDER[2],\n"
    "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
    "                ID[\"EPSG\",9122]]],\n"
    "    ID[\"EPSG\",4326]]");

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_geographic_with_PROJ4_extension) {
    auto wkt = "GEOGCS[\"WGS 84\",\n"
               "    DATUM[\"unknown\",\n"
               "        SPHEROID[\"WGS84\",6378137,298.257223563]],\n"
               "    PRIMEM[\"Greenwich\",0],\n"
               "    UNIT[\"degree\",0.0174532925199433],\n"
               "    EXTENSION[\"PROJ4\",\"+proj=longlat +foo=bar +wktext\"]]";
    auto obj = WKTParser().createFromWKT(wkt);

    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToWKT(
            WKTFormatter::create(WKTFormatter::Convention::WKT1_GDAL).get()),
        wkt);

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=longlat +foo=bar +wktext");

    EXPECT_TRUE(
        crs->exportToWKT(WKTFormatter::create().get()).find("EXTENSION") ==
        std::string::npos);

    EXPECT_TRUE(
        crs->exportToWKT(
               WKTFormatter::create(WKTFormatter::Convention::WKT1_ESRI).get())
            .find("EXTENSION") == std::string::npos);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_geocentric_with_PROJ4_extension) {
    auto wkt = "GEOCCS[\"WGS 84\",\n"
               "    DATUM[\"unknown\",\n"
               "        SPHEROID[\"WGS84\",6378137,298.257223563]],\n"
               "    PRIMEM[\"Greenwich\",0],\n"
               "    UNIT[\"Meter\",1],\n"
               "    AXIS[\"Geocentric X\",OTHER],\n"
               "    AXIS[\"Geocentric Y\",OTHER],\n"
               "    AXIS[\"Geocentric Z\",NORTH],\n"
               "    EXTENSION[\"PROJ4\",\"+proj=geocent +foo=bar +wktext\"]]";
    auto obj = WKTParser().createFromWKT(wkt);

    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToWKT(
            WKTFormatter::create(WKTFormatter::Convention::WKT1_GDAL).get()),
        wkt);

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=geocent +foo=bar +wktext");

    EXPECT_TRUE(
        crs->exportToWKT(WKTFormatter::create().get()).find("EXTENSION") ==
        std::string::npos);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_GEODCRS_EPSG_4326) {
    auto obj = WKTParser().createFromWKT("GEODCRS" + contentWKT2_EPSG_4326);
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkEPSG_4326(crs);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_long_GEODETICCRS_EPSG_4326) {
    auto obj = WKTParser().createFromWKT("GEODETICCRS" + contentWKT2_EPSG_4326);
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkEPSG_4326(crs);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_2018_GEOGCRS_EPSG_4326) {
    auto obj = WKTParser().createFromWKT("GEOGCRS" + contentWKT2_EPSG_4326);
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkEPSG_4326(crs);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_2018_long_GEOGRAPHICCRS_EPSG_4326) {
    auto obj =
        WKTParser().createFromWKT("GEOGRAPHICCRS" + contentWKT2_EPSG_4326);
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkEPSG_4326(crs);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_simplified_EPSG_4326) {
    auto obj = WKTParser().createFromWKT(
        "GEODCRS[\"WGS 84\",\n"
        "    DATUM[\"World Geodetic System 1984\",\n"
        "        ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
        "    CS[ellipsoidal,2],\n"
        "        AXIS[\"latitude (lat)\",north],\n" // test "name
                                                    // (abbreviation)"
        "        AXIS[\"longitude (lon)\",east],\n"
        "        UNIT[\"degree\",0.0174532925199433],\n"
        "    ID[\"EPSG\",4326]]");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkEPSG_4326(crs, true /* latlong */, false /* no EPSG codes */);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_GEODETICDATUM) {
    auto obj = WKTParser().createFromWKT(
        "GEODCRS[\"WGS 84\",\n"
        "    GEODETICDATUM[\"World Geodetic System 1984\",\n"
        "        ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
        "    CS[ellipsoidal,2],\n"
        "        AXIS[\"(lat)\",north],\n" // test "(abbreviation)"
        "        AXIS[\"(lon)\",east],\n"
        "        UNIT[\"degree\",0.0174532925199433],\n"
        "    ID[\"EPSG\",4326]]");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkEPSG_4326(crs, true /* latlong */, false /* no EPSG codes */);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_TRF) {
    auto obj = WKTParser().createFromWKT(
        "GEODCRS[\"WGS 84\",\n"
        "    TRF[\"World Geodetic System 1984\",\n"
        "        ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
        "    CS[ellipsoidal,2],\n"
        "        AXIS[\"latitude\",north],\n"
        "        AXIS[\"longitude\",east],\n"
        "        UNIT[\"degree\",0.0174532925199433],\n"
        "    ID[\"EPSG\",4326]]");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkEPSG_4326(crs, true /* latlong */, false /* no EPSG codes */);
}

// ---------------------------------------------------------------------------

static void checkEPSG_4979(GeographicCRSPtr crs) {
    ASSERT_EQ(crs->identifiers().size(), 1);
    EXPECT_EQ(crs->identifiers()[0]->code(), "4979");
    EXPECT_EQ(*(crs->identifiers()[0]->codeSpace()), "EPSG");
    EXPECT_EQ(crs->nameStr(), "WGS 84");

    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 3);
    EXPECT_EQ(cs->axisList()[0]->nameStr(), "Latitude");
    EXPECT_EQ(cs->axisList()[0]->abbreviation(), "lat");
    EXPECT_EQ(cs->axisList()[0]->direction(), AxisDirection::NORTH);

    EXPECT_EQ(cs->axisList()[1]->nameStr(), "Longitude");
    EXPECT_EQ(cs->axisList()[1]->abbreviation(), "lon");
    EXPECT_EQ(cs->axisList()[1]->direction(), AxisDirection::EAST);

    EXPECT_EQ(cs->axisList()[2]->nameStr(), "Ellipsoidal height");
    EXPECT_EQ(cs->axisList()[2]->abbreviation(), "h");
    EXPECT_EQ(cs->axisList()[2]->direction(), AxisDirection::UP);

    auto datum = crs->datum();
    EXPECT_EQ(datum->nameStr(), "World Geodetic System 1984");

    auto ellipsoid = datum->ellipsoid();
    EXPECT_EQ(ellipsoid->semiMajorAxis().value(), 6378137.0);
    EXPECT_EQ(ellipsoid->semiMajorAxis().unit(), UnitOfMeasure::METRE);
    EXPECT_EQ(ellipsoid->inverseFlattening()->value(), 298.257223563);
    EXPECT_EQ(ellipsoid->nameStr(), "WGS 84");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_EPSG_4979) {
    auto obj = WKTParser().createFromWKT(
        "GEODCRS[\"WGS 84\",\n"
        "    DATUM[\"World Geodetic System 1984\",\n"
        "        ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
        "    CS[ellipsoidal,3],\n"
        "        AXIS[\"latitude\",north,\n"
        "            UNIT[\"degree\",0.0174532925199433]],\n"
        "        AXIS[\"longitude\",east,\n"
        "            UNIT[\"degree\",0.0174532925199433]],\n"
        "        AXIS[\"ellipsoidal height\",up,\n"
        "            UNIT[\"metre\",1]],\n"
        "    ID[\"EPSG\",4979]]");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkEPSG_4979(crs);
}

// ---------------------------------------------------------------------------

static void checkGeocentric(GeodeticCRSPtr crs) {
    // Explicitly check this is NOT a GeographicCRS
    EXPECT_TRUE(!dynamic_cast<GeographicCRS *>(crs.get()));

    EXPECT_EQ(crs->nameStr(), "WGS 84 (geocentric)");

    EXPECT_TRUE(crs->isGeocentric());
    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 3);

    EXPECT_EQ(cs->axisList()[0]->nameStr(), "Geocentric X");
    EXPECT_EQ(cs->axisList()[0]->abbreviation(), "X");
    EXPECT_EQ(cs->axisList()[0]->direction(), AxisDirection::GEOCENTRIC_X);

    EXPECT_EQ(cs->axisList()[1]->nameStr(), "Geocentric Y");
    EXPECT_EQ(cs->axisList()[1]->abbreviation(), "Y");
    EXPECT_EQ(cs->axisList()[1]->direction(), AxisDirection::GEOCENTRIC_Y);

    EXPECT_EQ(cs->axisList()[2]->nameStr(), "Geocentric Z");
    EXPECT_EQ(cs->axisList()[2]->abbreviation(), "Z");
    EXPECT_EQ(cs->axisList()[2]->direction(), AxisDirection::GEOCENTRIC_Z);

    auto datum = crs->datum();
    EXPECT_EQ(datum->nameStr(), "World Geodetic System 1984");

    auto ellipsoid = datum->ellipsoid();
    EXPECT_EQ(ellipsoid->semiMajorAxis().value(), 6378137.0);
    EXPECT_EQ(ellipsoid->semiMajorAxis().unit(), UnitOfMeasure::METRE);
    EXPECT_EQ(ellipsoid->inverseFlattening()->value(), 298.257223563);
    EXPECT_EQ(ellipsoid->nameStr(), "WGS 84");

    auto primem = datum->primeMeridian();
    ASSERT_EQ(primem->longitude().unit(), UnitOfMeasure::DEGREE);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_geocentric) {
    auto wkt = "GEODCRS[\"WGS 84 (geocentric)\",\n"
               "    DATUM[\"World Geodetic System 1984\",\n"
               "        ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]],\n"
               "            ID[\"EPSG\",7030]],\n"
               "        ID[\"EPSG\",6326]],\n"
               "    PRIMEM[\"Greenwich\",0,\n"
               "        ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "            ID[\"EPSG\",9122]],\n"
               "        ID[\"EPSG\",8901]],\n"
               "    CS[Cartesian,3],\n"
               "        AXIS[\"(X)\",geocentricX,\n"
               "            ORDER[1],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]],\n"
               "        AXIS[\"(Y)\",geocentricY,\n"
               "            ORDER[2],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]],\n"
               "        AXIS[\"(Z)\",geocentricZ,\n"
               "            ORDER[3],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]],\n"
               "    ID[\"EPSG\",4328]]";
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkGeocentric(crs);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_simplified_geocentric) {
    auto wkt = "GEODCRS[\"WGS 84 (geocentric)\",\n"
               "    DATUM[\"World Geodetic System 1984\",\n"
               "        ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
               "    CS[Cartesian,3],\n"
               "        AXIS[\"(X)\",geocentricX],\n"
               "        AXIS[\"(Y)\",geocentricY],\n"
               "        AXIS[\"(Z)\",geocentricZ],\n"
               "        UNIT[\"metre\",1],\n"
               "    ID[\"EPSG\",4328]]";
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkGeocentric(crs);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_geocentric) {
    auto wkt = "GEOCCS[\"WGS 84 (geocentric)\",\n"
               "    DATUM[\"WGS_1984\",\n"
               "        SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "            AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "        AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "    PRIMEM[\"Greenwich\",0,\n"
               "        AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]],\n"
               "    AXIS[\"Geocentric X\",OTHER],\n"
               "    AXIS[\"Geocentric Y\",OTHER],\n"
               "    AXIS[\"Geocentric Z\",NORTH],\n"
               "    AUTHORITY[\"EPSG\",\"4328\"]]";
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkGeocentric(crs);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_geocentric_with_z_OTHER) {
    auto wkt = "GEOCCS[\"WGS 84 (geocentric)\",\n"
               "    DATUM[\"WGS_1984\",\n"
               "        SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "            AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "        AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "    PRIMEM[\"Greenwich\",0,\n"
               "        AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]],\n"
               "    AXIS[\"Geocentric X\",OTHER],\n"
               "    AXIS[\"Geocentric Y\",OTHER],\n"
               "    AXIS[\"Geocentric Z\",OTHER],\n"
               "    AUTHORITY[\"EPSG\",\"4328\"]]";
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkGeocentric(crs);
}

// ---------------------------------------------------------------------------

static void checkProjected(ProjectedCRSPtr crs, bool checkEPSGCodes = true) {
    EXPECT_EQ(crs->nameStr(), "WGS 84 / UTM zone 31N");
    ASSERT_EQ(crs->identifiers().size(), 1);
    EXPECT_EQ(crs->identifiers()[0]->code(), "32631");
    EXPECT_EQ(*(crs->identifiers()[0]->codeSpace()), "EPSG");

    auto geogCRS = nn_dynamic_pointer_cast<GeographicCRS>(crs->baseCRS());
    ASSERT_TRUE(geogCRS != nullptr);
    checkEPSG_4326(NN_CHECK_ASSERT(geogCRS), true, checkEPSGCodes);

    auto conversion = crs->derivingConversion();
    EXPECT_EQ(conversion->nameStr(), "UTM zone 31N");
    auto method = conversion->method();
    EXPECT_EQ(method->nameStr(), "Transverse Mercator");
    auto values = conversion->parameterValues();
    ASSERT_EQ(values.size(), 5);
    {
        const auto &opParamvalue =
            nn_dynamic_pointer_cast<OperationParameterValue>(values[0]);
        ASSERT_TRUE(opParamvalue);
        const auto &paramName = opParamvalue->parameter()->nameStr();
        const auto &parameterValue = opParamvalue->parameterValue();
        EXPECT_EQ(paramName, "Latitude of natural origin");
        EXPECT_EQ(parameterValue->type(), ParameterValue::Type::MEASURE);
        auto measure = parameterValue->value();
        EXPECT_EQ(measure.unit(), UnitOfMeasure::DEGREE);
        EXPECT_EQ(measure.value(), 0);
    }
    {
        const auto &opParamvalue =
            nn_dynamic_pointer_cast<OperationParameterValue>(values[1]);
        ASSERT_TRUE(opParamvalue);
        const auto &paramName = opParamvalue->parameter()->nameStr();
        const auto &parameterValue = opParamvalue->parameterValue();
        EXPECT_EQ(paramName, "Longitude of natural origin");
        EXPECT_EQ(parameterValue->type(), ParameterValue::Type::MEASURE);
        auto measure = parameterValue->value();
        EXPECT_EQ(measure.unit(), UnitOfMeasure::DEGREE);
        EXPECT_EQ(measure.value(), 3);
    }
    {
        const auto &opParamvalue =
            nn_dynamic_pointer_cast<OperationParameterValue>(values[2]);
        ASSERT_TRUE(opParamvalue);
        const auto &paramName = opParamvalue->parameter()->nameStr();
        const auto &parameterValue = opParamvalue->parameterValue();
        EXPECT_EQ(paramName, "Scale factor at natural origin");
        EXPECT_EQ(parameterValue->type(), ParameterValue::Type::MEASURE);
        auto measure = parameterValue->value();
        EXPECT_EQ(measure.unit(), UnitOfMeasure::SCALE_UNITY);
        EXPECT_EQ(measure.value(), 0.9996);
    }
    {
        const auto &opParamvalue =
            nn_dynamic_pointer_cast<OperationParameterValue>(values[3]);
        ASSERT_TRUE(opParamvalue);
        const auto &paramName = opParamvalue->parameter()->nameStr();
        const auto &parameterValue = opParamvalue->parameterValue();
        EXPECT_EQ(paramName, "False easting");
        EXPECT_EQ(parameterValue->type(), ParameterValue::Type::MEASURE);
        auto measure = parameterValue->value();
        EXPECT_EQ(measure.unit(), UnitOfMeasure::METRE);
        EXPECT_EQ(measure.value(), 500000);
    }
    {
        const auto &opParamvalue =
            nn_dynamic_pointer_cast<OperationParameterValue>(values[4]);
        ASSERT_TRUE(opParamvalue);
        const auto &paramName = opParamvalue->parameter()->nameStr();
        const auto &parameterValue = opParamvalue->parameterValue();
        EXPECT_EQ(paramName, "False northing");
        EXPECT_EQ(parameterValue->type(), ParameterValue::Type::MEASURE);
        auto measure = parameterValue->value();
        EXPECT_EQ(measure.unit(), UnitOfMeasure::METRE);
        EXPECT_EQ(measure.value(), 0);
    }

    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 2);

    EXPECT_EQ(cs->axisList()[0]->nameStr(), "Easting");
    EXPECT_EQ(cs->axisList()[0]->abbreviation(), "E");
    EXPECT_EQ(cs->axisList()[0]->direction(), AxisDirection::EAST);

    EXPECT_EQ(cs->axisList()[1]->nameStr(), "Northing");
    EXPECT_EQ(cs->axisList()[1]->abbreviation(), "N");
    EXPECT_EQ(cs->axisList()[1]->direction(), AxisDirection::NORTH);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_projected) {
    auto wkt = "PROJCS[\"WGS 84 / UTM zone 31N\",\n"
               "    GEOGCS[\"WGS 84\",\n"
               "        DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "        UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9122\"]],\n"
               "        AUTHORITY[\"EPSG\",\"4326\"]],\n"
               "    PROJECTION[\"Transverse_Mercator\"],\n"
               "    PARAMETER[\"latitude_of_origin\",0],\n"
               "    PARAMETER[\"central_meridian\",3],\n"
               "    PARAMETER[\"scale_factor\",0.9996],\n"
               "    PARAMETER[\"false_easting\",500000],\n"
               "    PARAMETER[\"false_northing\",0],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]],\n"
               "    AXIS[\"(E)\",EAST],\n"
               "    AXIS[\"(N)\",NORTH],\n"
               "    AUTHORITY[\"EPSG\",\"32631\"]]";
    auto obj = WKTParser()
                   .attachDatabaseContext(DatabaseContext::create())
                   .createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkProjected(crs);

    EXPECT_TRUE(!crs->baseCRS()->identifiers().empty());
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_projected_no_axis) {
    auto wkt = "PROJCS[\"WGS 84 / UTM zone 31N\",\n"
               "    GEOGCS[\"WGS 84\",\n"
               "        DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "        UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9122\"]],\n"
               "        AXIS[\"latitude\",NORTH],\n"
               "        AXIS[\"longitude\",EAST],\n"
               "        AUTHORITY[\"EPSG\",\"4326\"]],\n"
               "    PROJECTION[\"Transverse_Mercator\"],\n"
               "    PARAMETER[\"latitude_of_origin\",0],\n"
               "    PARAMETER[\"central_meridian\",3],\n"
               "    PARAMETER[\"scale_factor\",0.9996],\n"
               "    PARAMETER[\"false_easting\",500000],\n"
               "    PARAMETER[\"false_northing\",0],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]],\n"
               "    AUTHORITY[\"EPSG\",\"32631\"]]";
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkProjected(crs);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_projected_wrong_axis_geogcs) {
    auto wkt = "PROJCS[\"WGS 84 / UTM zone 31N\",\n"
               "    GEOGCS[\"WGS 84\",\n"
               "        DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "        UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9122\"]],\n"
               "        AXIS[\"longitude\",EAST],\n"
               "        AXIS[\"latitude\",NORTH],\n"
               "        AUTHORITY[\"EPSG\",\"4326\"]],\n"
               "    PROJECTION[\"Transverse_Mercator\"],\n"
               "    PARAMETER[\"latitude_of_origin\",0],\n"
               "    PARAMETER[\"central_meridian\",3],\n"
               "    PARAMETER[\"scale_factor\",0.9996],\n"
               "    PARAMETER[\"false_easting\",500000],\n"
               "    PARAMETER[\"false_northing\",0],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]],\n"
               "    AUTHORITY[\"EPSG\",\"32631\"]]";
    WKTParser parser;
    parser.setStrict(false).attachDatabaseContext(DatabaseContext::create());
    auto obj = parser.createFromWKT(wkt);
    EXPECT_TRUE(!parser.warningList().empty());
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_TRUE(crs->baseCRS()->identifiers().empty());

    auto cs = crs->baseCRS()->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 2);
    EXPECT_EQ(cs->axisList()[0]->direction(), AxisDirection::EAST);
    EXPECT_EQ(cs->axisList()[1]->direction(), AxisDirection::NORTH);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_projected_with_PROJ4_extension) {
    auto wkt = "PROJCS[\"unnamed\",\n"
               "    GEOGCS[\"WGS 84\",\n"
               "        DATUM[\"unknown\",\n"
               "            SPHEROID[\"WGS84\",6378137,298.257223563]],\n"
               "        PRIMEM[\"Greenwich\",0],\n"
               "        UNIT[\"degree\",0.0174532925199433]],\n"
               "    PROJECTION[\"Mercator_1SP\"],\n"
               "    PARAMETER[\"central_meridian\",0],\n"
               "    PARAMETER[\"scale_factor\",1],\n"
               "    PARAMETER[\"false_easting\",0],\n"
               "    PARAMETER[\"false_northing\",0],\n"
               "    UNIT[\"Meter\",1],\n"
               "    AXIS[\"Easting\",EAST],\n"
               "    AXIS[\"Northing\",NORTH],\n"
               "    EXTENSION[\"PROJ4\",\"+proj=merc +wktext\"]]";
    auto obj = WKTParser().createFromWKT(wkt);

    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToWKT(
            WKTFormatter::create(WKTFormatter::Convention::WKT1_GDAL).get()),
        wkt);

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=merc +wktext");

    EXPECT_TRUE(
        crs->exportToWKT(WKTFormatter::create().get()).find("EXTENSION") ==
        std::string::npos);

    EXPECT_TRUE(
        crs->exportToWKT(
               WKTFormatter::create(WKTFormatter::Convention::WKT1_ESRI).get())
            .find("EXTENSION") == std::string::npos);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_Mercator_1SP_with_latitude_origin_0) {
    auto wkt = "PROJCS[\"unnamed\",\n"
               "    GEOGCS[\"WGS 84\",\n"
               "        DATUM[\"unknown\",\n"
               "            SPHEROID[\"WGS84\",6378137,298.257223563]],\n"
               "        PRIMEM[\"Greenwich\",0],\n"
               "        UNIT[\"degree\",0.0174532925199433]],\n"
               "    PROJECTION[\"Mercator_1SP\"],\n"
               "    PARAMETER[\"latitude_of_origin\",0],\n"
               "    PARAMETER[\"central_meridian\",0],\n"
               "    PARAMETER[\"scale_factor\",1],\n"
               "    PARAMETER[\"false_easting\",0],\n"
               "    PARAMETER[\"false_northing\",0],\n"
               "    UNIT[\"Meter\",1],\n"
               "    AXIS[\"Easting\",EAST],\n"
               "    AXIS[\"Northing\",NORTH]]";
    auto obj = WKTParser().createFromWKT(wkt);

    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    auto got_wkt = crs->exportToWKT(
        WKTFormatter::create(WKTFormatter::Convention::WKT1_GDAL).get());
    EXPECT_TRUE(got_wkt.find("Mercator_1SP") != std::string::npos) << got_wkt;
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_krovak_south_west) {
    auto wkt =
        "PROJCS[\"S-JTSK / Krovak\","
        "    GEOGCS[\"S-JTSK\","
        "        DATUM[\"System_Jednotne_Trigonometricke_Site_Katastralni\","
        "            SPHEROID[\"Bessel 1841\",6377397.155,299.1528128,"
        "                AUTHORITY[\"EPSG\",\"7004\"]],"
        "            AUTHORITY[\"EPSG\",\"6156\"]],"
        "        PRIMEM[\"Greenwich\",0,"
        "            AUTHORITY[\"EPSG\",\"8901\"]],"
        "        UNIT[\"degree\",0.0174532925199433,"
        "            AUTHORITY[\"EPSG\",\"9122\"]],"
        "        AUTHORITY[\"EPSG\",\"4156\"]],"
        "    PROJECTION[\"Krovak\"],"
        "    PARAMETER[\"latitude_of_center\",49.5],"
        "    PARAMETER[\"longitude_of_center\",24.83333333333333],"
        "    PARAMETER[\"azimuth\",30.28813972222222],"
        "    PARAMETER[\"pseudo_standard_parallel_1\",78.5],"
        "    PARAMETER[\"scale_factor\",0.9999],"
        "    PARAMETER[\"false_easting\",0],"
        "    PARAMETER[\"false_northing\",0],"
        "    UNIT[\"metre\",1,"
        "        AUTHORITY[\"EPSG\",\"9001\"]],"
        "    AXIS[\"X\",SOUTH],"
        "    AXIS[\"Y\",WEST],"
        "    AUTHORITY[\"EPSG\",\"5513\"]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->derivingConversion()->method()->nameStr(), "Krovak");

    auto expected_wkt2 =
        "PROJCRS[\"S-JTSK / Krovak\",\n"
        "    BASEGEODCRS[\"S-JTSK\",\n"
        "        DATUM[\"System_Jednotne_Trigonometricke_Site_Katastralni\",\n"
        "            ELLIPSOID[\"Bessel 1841\",6377397.155,299.1528128,\n"
        "                LENGTHUNIT[\"metre\",1]]],\n"
        "        PRIMEM[\"Greenwich\",0,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "    CONVERSION[\"unnamed\",\n"
        "        METHOD[\"Krovak\",\n"
        "            ID[\"EPSG\",9819]],\n"
        "        PARAMETER[\"Latitude of projection centre\",49.5,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8811]],\n"
        "        PARAMETER[\"Longitude of origin\",24.8333333333333,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8833]],\n"
        "        PARAMETER[\"Co-latitude of cone axis\",30.2881397222222,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",1036]],\n"
        "        PARAMETER[\"Latitude of pseudo standard parallel\",78.5,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8818]],\n"
        "        PARAMETER[\"Scale factor on pseudo standard "
        "parallel\",0.9999,\n"
        "            SCALEUNIT[\"unity\",1],\n"
        "            ID[\"EPSG\",8819]],\n"
        "        PARAMETER[\"False easting\",0,\n"
        "            LENGTHUNIT[\"metre\",1],\n"
        "            ID[\"EPSG\",8806]],\n"
        "        PARAMETER[\"False northing\",0,\n"
        "            LENGTHUNIT[\"metre\",1],\n"
        "            ID[\"EPSG\",8807]]],\n"
        "    CS[Cartesian,2],\n"
        "        AXIS[\"x\",south,\n"
        "            ORDER[1],\n"
        "            LENGTHUNIT[\"metre\",1]],\n"
        "        AXIS[\"y\",west,\n"
        "            ORDER[2],\n"
        "            LENGTHUNIT[\"metre\",1]],\n"
        "    ID[\"EPSG\",5513]]";

    EXPECT_EQ(crs->exportToWKT(WKTFormatter::create().get()), expected_wkt2);

    auto projString =
        crs->exportToPROJString(PROJStringFormatter::create().get());
    auto expectedPROJString =
        "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
        "+step +proj=krovak +axis=swu +lat_0=49.5 "
        "+lon_0=24.8333333333333 +alpha=30.2881397222222 "
        "+k=0.9999 +x_0=0 +y_0=0 +ellps=bessel";
    EXPECT_EQ(projString, expectedPROJString);

    obj = PROJStringParser().createFromPROJString(projString);
    crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    auto wkt2 = crs->exportToWKT(WKTFormatter::create().get());
    EXPECT_TRUE(wkt2.find("METHOD[\"Krovak\"") != std::string::npos) << wkt2;
    EXPECT_TRUE(
        wkt2.find("PARAMETER[\"Latitude of pseudo standard parallel\",78.5,") !=
        std::string::npos)
        << wkt2;
    EXPECT_TRUE(
        wkt2.find("PARAMETER[\"Co-latitude of cone axis\",30.2881397222222,") !=
        std::string::npos)
        << wkt2;
    EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
              expectedPROJString);

    obj = PROJStringParser().createFromPROJString(
        "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
        "+step +proj=krovak +lat_0=49.5 "
        "+lon_0=24.8333333333333 +alpha=30.2881397222222 "
        "+k=0.9999 +x_0=0 +y_0=0 +ellps=bessel "
        "+step +proj=axisswap +order=-2,-1");
    crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    wkt2 = crs->exportToWKT(WKTFormatter::create().get());
    EXPECT_TRUE(wkt2.find("METHOD[\"Krovak\"") != std::string::npos) << wkt2;
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_krovak_north_oriented) {
    auto wkt =
        "PROJCS[\"S-JTSK / Krovak East North\","
        "    GEOGCS[\"S-JTSK\","
        "        DATUM[\"System_Jednotne_Trigonometricke_Site_Katastralni\","
        "            SPHEROID[\"Bessel 1841\",6377397.155,299.1528128,"
        "                AUTHORITY[\"EPSG\",\"7004\"]],"
        "            AUTHORITY[\"EPSG\",\"6156\"]],"
        "        PRIMEM[\"Greenwich\",0,"
        "            AUTHORITY[\"EPSG\",\"8901\"]],"
        "        UNIT[\"degree\",0.0174532925199433,"
        "            AUTHORITY[\"EPSG\",\"9122\"]],"
        "        AUTHORITY[\"EPSG\",\"4156\"]],"
        "    PROJECTION[\"Krovak\"],"
        "    PARAMETER[\"latitude_of_center\",49.5],"
        "    PARAMETER[\"longitude_of_center\",24.83333333333333],"
        "    PARAMETER[\"azimuth\",30.28813972222222],"
        "    PARAMETER[\"pseudo_standard_parallel_1\",78.5],"
        "    PARAMETER[\"scale_factor\",0.9999],"
        "    PARAMETER[\"false_easting\",0],"
        "    PARAMETER[\"false_northing\",0],"
        "    UNIT[\"metre\",1,"
        "        AUTHORITY[\"EPSG\",\"9001\"]],"
        "    AXIS[\"X\",EAST],"
        "    AXIS[\"Y\",NORTH],"
        "    AUTHORITY[\"EPSG\",\"5514\"]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->derivingConversion()->method()->nameStr(),
              "Krovak (North Orientated)");

    EXPECT_EQ(
        crs->exportToWKT(WKTFormatter::create().get()),
        "PROJCRS[\"S-JTSK / Krovak East North\",\n"
        "    BASEGEODCRS[\"S-JTSK\",\n"
        "        DATUM[\"System_Jednotne_Trigonometricke_Site_Katastralni\",\n"
        "            ELLIPSOID[\"Bessel 1841\",6377397.155,299.1528128,\n"
        "                LENGTHUNIT[\"metre\",1]]],\n"
        "        PRIMEM[\"Greenwich\",0,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "    CONVERSION[\"unnamed\",\n"
        "        METHOD[\"Krovak (North Orientated)\",\n"
        "            ID[\"EPSG\",1041]],\n"
        "        PARAMETER[\"Latitude of projection centre\",49.5,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8811]],\n"
        "        PARAMETER[\"Longitude of origin\",24.8333333333333,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8833]],\n"
        "        PARAMETER[\"Co-latitude of cone axis\",30.2881397222222,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",1036]],\n"
        "        PARAMETER[\"Latitude of pseudo standard parallel\",78.5,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8818]],\n"
        "        PARAMETER[\"Scale factor on pseudo standard "
        "parallel\",0.9999,\n"
        "            SCALEUNIT[\"unity\",1],\n"
        "            ID[\"EPSG\",8819]],\n"
        "        PARAMETER[\"False easting\",0,\n"
        "            LENGTHUNIT[\"metre\",1],\n"
        "            ID[\"EPSG\",8806]],\n"
        "        PARAMETER[\"False northing\",0,\n"
        "            LENGTHUNIT[\"metre\",1],\n"
        "            ID[\"EPSG\",8807]]],\n"
        "    CS[Cartesian,2],\n"
        "        AXIS[\"x\",east,\n"
        "            ORDER[1],\n"
        "            LENGTHUNIT[\"metre\",1]],\n"
        "        AXIS[\"y\",north,\n"
        "            ORDER[2],\n"
        "            LENGTHUNIT[\"metre\",1]],\n"
        "    ID[\"EPSG\",5514]]");

    EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
              "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
              "+step +proj=krovak +lat_0=49.5 +lon_0=24.8333333333333 "
              "+alpha=30.2881397222222 +k=0.9999 +x_0=0 +y_0=0 +ellps=bessel");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_polar_stereographic_latitude_of_origin_70) {
    auto wkt = "PROJCS[\"unknown\",\n"
               "    GEOGCS[\"unknown\",\n"
               "        DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "        UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9122\"]]],\n"
               "    PROJECTION[\"Polar_Stereographic\"],\n"
               "    PARAMETER[\"latitude_of_origin\",70],\n"
               "    PARAMETER[\"central_meridian\",2],\n"
               "    PARAMETER[\"false_easting\",3],\n"
               "    PARAMETER[\"false_northing\",4],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    auto projString = crs->exportToPROJString(
        PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
            .get());
    auto expectedPROJString = "+proj=stere +lat_0=90 +lat_ts=70 +lon_0=2 "
                              "+x_0=3 +y_0=4 +datum=WGS84 +units=m +no_defs";
    EXPECT_EQ(projString, expectedPROJString);

    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->nameStr(), "Easting");
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->direction(),
              AxisDirection::SOUTH);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[1]->nameStr(), "Northing");
    EXPECT_EQ(crs->coordinateSystem()->axisList()[1]->direction(),
              AxisDirection::SOUTH);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_polar_stereographic_latitude_of_origin_minus_70) {
    auto wkt = "PROJCS[\"unknown\",\n"
               "    GEOGCS[\"unknown\",\n"
               "        DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "        UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9122\"]]],\n"
               "    PROJECTION[\"Polar_Stereographic\"],\n"
               "    PARAMETER[\"latitude_of_origin\",-70],\n"
               "    PARAMETER[\"central_meridian\",2],\n"
               "    PARAMETER[\"false_easting\",3],\n"
               "    PARAMETER[\"false_northing\",4],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->nameStr(), "Easting");
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->direction(),
              AxisDirection::NORTH);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[1]->nameStr(), "Northing");
    EXPECT_EQ(crs->coordinateSystem()->axisList()[1]->direction(),
              AxisDirection::NORTH);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_polar_stereographic_latitude_of_origin_90) {
    auto wkt = "PROJCS[\"unknown\",\n"
               "    GEOGCS[\"unknown\",\n"
               "        DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "        UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9122\"]]],\n"
               "    PROJECTION[\"Polar_Stereographic\"],\n"
               "    PARAMETER[\"latitude_of_origin\",90],\n"
               "    PARAMETER[\"central_meridian\",2],\n"
               "    PARAMETER[\"false_easting\",3],\n"
               "    PARAMETER[\"false_northing\",4],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    auto projString = crs->exportToPROJString(
        PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
            .get());
    auto expectedPROJString = "+proj=stere +lat_0=90 +lat_ts=90 +lon_0=2 "
                              "+x_0=3 +y_0=4 +datum=WGS84 +units=m +no_defs";
    EXPECT_EQ(projString, expectedPROJString);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_polar_stereographic_latitude_of_origin_90_scale_factor_1) {
    auto wkt = "PROJCS[\"unknown\",\n"
               "    GEOGCS[\"unknown\",\n"
               "        DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "        UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9122\"]]],\n"
               "    PROJECTION[\"Polar_Stereographic\"],\n"
               "    PARAMETER[\"latitude_of_origin\",90],\n"
               "    PARAMETER[\"central_meridian\",2],\n"
               "    PARAMETER[\"scale_factor\",1],\n"
               "    PARAMETER[\"false_easting\",3],\n"
               "    PARAMETER[\"false_northing\",4],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    auto projString = crs->exportToPROJString(
        PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
            .get());
    auto expectedPROJString = "+proj=stere +lat_0=90 +lat_ts=90 +lon_0=2 "
                              "+x_0=3 +y_0=4 +datum=WGS84 +units=m +no_defs";
    EXPECT_EQ(projString, expectedPROJString);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_polar_stereographic_scale_factor) {
    auto wkt = "PROJCS[\"unknown\",\n"
               "    GEOGCS[\"unknown\",\n"
               "        DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "        UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9122\"]]],\n"
               "    PROJECTION[\"Polar_Stereographic\"],\n"
               "    PARAMETER[\"latitude_of_origin\",90],\n"
               "    PARAMETER[\"central_meridian\",2],\n"
               "    PARAMETER[\"scale_factor\",0.99],\n"
               "    PARAMETER[\"false_easting\",3],\n"
               "    PARAMETER[\"false_northing\",4],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    auto projString = crs->exportToPROJString(
        PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
            .get());
    auto expectedPROJString = "+proj=stere +lat_0=90 +lon_0=2 +k=0.99 +x_0=3 "
                              "+y_0=4 +datum=WGS84 +units=m +no_defs";
    EXPECT_EQ(projString, expectedPROJString);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_Spherical_Cross_Track_Height) {
    auto wkt = "PROJCS[\"unknown\",\n"
               "    GEOGCS[\"unknown\",\n"
               "        DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "        UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9122\"]]],\n"
               "    PROJECTION[\"Spherical_Cross_Track_Height\"],\n"
               "    PARAMETER[\"peg_point_latitude\",1],\n"
               "    PARAMETER[\"peg_point_longitude\",2],\n"
               "    PARAMETER[\"peg_point_heading\",3],\n"
               "    PARAMETER[\"peg_point_height\",4],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    auto projString = crs->exportToPROJString(
        PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
            .get());
    auto expectedPROJString = "+proj=sch +plat_0=1 +plon_0=2 +phdg_0=3 +h_0=4 "
                              "+datum=WGS84 +units=m +no_defs";
    EXPECT_EQ(projString, expectedPROJString);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_projected) {
    auto wkt = "PROJCRS[\"WGS 84 / UTM zone 31N\",\n"
               "    BASEGEODCRS[\"WGS 84\",\n"
               "        DATUM[\"World Geodetic System 1984\",\n"
               "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
               "                LENGTHUNIT[\"metre\",1,\n"
               "                    ID[\"EPSG\",9001]],\n"
               "                ID[\"EPSG\",7030]],\n"
               "            ID[\"EPSG\",6326]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]],\n"
               "            ID[\"EPSG\",8901]]],\n"
               "    CONVERSION[\"UTM zone 31N\",\n"
               "        METHOD[\"Transverse Mercator\",\n"
               "            ID[\"EPSG\",9807]],\n"
               "        PARAMETER[\"Latitude of natural origin\",0,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]],\n"
               "            ID[\"EPSG\",8801]],\n"
               "        PARAMETER[\"Longitude of natural origin\",3,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]],\n"
               "            ID[\"EPSG\",8802]],\n"
               "        PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
               "            SCALEUNIT[\"unity\",1,\n"
               "                ID[\"EPSG\",9201]],\n"
               "            ID[\"EPSG\",8805]],\n"
               "        PARAMETER[\"False easting\",500000,\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]],\n"
               "            ID[\"EPSG\",8806]],\n"
               "        PARAMETER[\"False northing\",0,\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]],\n"
               "            ID[\"EPSG\",8807]],\n"
               "        ID[\"EPSG\",16031]],\n"
               "    CS[Cartesian,2],\n"
               "        AXIS[\"(E)\",east,\n"
               "            ORDER[1],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]],\n"
               "        AXIS[\"(N)\",north,\n"
               "            ORDER[2],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]],\n"
               "    ID[\"EPSG\",32631]]";
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkProjected(crs, /*checkEPSGCodes = */ false);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_2018_simplified_projected) {
    auto wkt = "PROJCRS[\"WGS 84 / UTM zone 31N\",\n"
               "    BASEGEOGCRS[\"WGS 84\",\n"
               "        DATUM[\"World Geodetic System 1984\",\n"
               "            ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
               "        UNIT[\"degree\",0.0174532925199433]],\n"
               "    CONVERSION[\"UTM zone 31N\",\n"
               "        METHOD[\"Transverse Mercator\"],\n"
               "        PARAMETER[\"Latitude of natural origin\",0],\n"
               "        PARAMETER[\"Longitude of natural origin\",3],\n"
               "        PARAMETER[\"Scale factor at natural origin\",0.9996],\n"
               "        PARAMETER[\"False easting\",500000],\n"
               "        PARAMETER[\"False northing\",0]],\n"
               "    CS[Cartesian,2],\n"
               "        AXIS[\"(E)\",east],\n"
               "        AXIS[\"(N)\",north],\n"
               "        UNIT[\"metre\",1],\n"
               "    ID[\"EPSG\",32631]]";
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    checkProjected(crs, /*checkEPSGCodes = */ false);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_2018_projected_3D) {
    auto wkt =
        "PROJCRS[\"WGS 84 (G1762) / UTM zone 31N 3D\","
        "    BASEGEOGCRS[\"WGS 84\","
        "        DATUM[\"World Geodetic System of 1984 (G1762)\","
        "        ELLIPSOID[\"WGS 84\",6378137,298.257223563,"
        "           LENGTHUNIT[\"metre\",1.0]]]],"
        "    CONVERSION[\"Some conversion 3D\","
        "        METHOD[\"Transverse Mercator (3D)\"],"
        "        PARAMETER[\"Latitude of origin\",0.0,"
        "            ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "        PARAMETER[\"Longitude of origin\",3.0,"
        "            ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "        PARAMETER[\"Scale factor\",1,SCALEUNIT[\"unity\",1.0]],"
        "        PARAMETER[\"False easting\",0.0,"
        "           LENGTHUNIT[\"metre\",1.0]],"
        "        PARAMETER[\"False northing\",0.0,LENGTHUNIT[\"metre\",1.0]]],"
        "        CS[Cartesian,3],"
        "            AXIS[\"(E)\",east,ORDER[1]],"
        "            AXIS[\"(N)\",north,ORDER[2]],"
        "            AXIS[\"ellipsoidal height (h)\",up,ORDER[3]],"
        "            LENGTHUNIT[\"metre\",1.0]"
        "]";
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=tmerc +lat_0=0 +lon_0=3 +k=1 +x_0=0 +y_0=0 +ellps=WGS84 "
        "+units=m +no_defs");

    EXPECT_THROW(
        crs->exportToWKT(
            WKTFormatter::create(WKTFormatter::Convention::WKT2_2015).get()),
        FormattingException);

    EXPECT_NO_THROW(crs->exportToWKT(
        WKTFormatter::create(WKTFormatter::Convention::WKT2_2018).get()));
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt2_2018_projected_utm_3D) {
    // Example from WKT2:2018
    auto wkt =
        "PROJCRS[\"WGS 84 (G1762) / UTM zone 31N 3D\","
        "    BASEGEOGCRS[\"WGS 84\","
        "        DATUM[\"World Geodetic System of 1984 (G1762)\","
        "        ELLIPSOID[\"WGS 84\",6378137,298.257223563,"
        "           LENGTHUNIT[\"metre\",1.0]]]],"
        "    CONVERSION[\"UTM zone 31N 3D\","
        "        METHOD[\"Transverse Mercator (3D)\"],"
        "        PARAMETER[\"Latitude of origin\",0.0,"
        "            ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "        PARAMETER[\"Longitude of origin\",3.0,"
        "            ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "        PARAMETER[\"Scale factor\",0.9996,SCALEUNIT[\"unity\",1.0]],"
        "        PARAMETER[\"False easting\",500000.0,"
        "           LENGTHUNIT[\"metre\",1.0]],"
        "        PARAMETER[\"False northing\",0.0,LENGTHUNIT[\"metre\",1.0]]],"
        "        CS[Cartesian,3],"
        "            AXIS[\"(E)\",east,ORDER[1]],"
        "            AXIS[\"(N)\",north,ORDER[2]],"
        "            AXIS[\"ellipsoidal height (h)\",up,ORDER[3]],"
        "            LENGTHUNIT[\"metre\",1.0]"
        "]";
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=utm +zone=31 +ellps=WGS84 +units=m +no_defs");

    EXPECT_THROW(
        crs->exportToWKT(
            WKTFormatter::create(WKTFormatter::Convention::WKT2_2015).get()),
        FormattingException);

    EXPECT_NO_THROW(crs->exportToWKT(
        WKTFormatter::create(WKTFormatter::Convention::WKT2_2018).get()));
}

// ---------------------------------------------------------------------------

TEST(crs, projected_angular_unit_from_primem) {
    auto obj = WKTParser().createFromWKT(
        "PROJCRS[\"NTF (Paris) / Lambert Nord France\",\n"
        "  BASEGEODCRS[\"NTF (Paris)\",\n"
        "    DATUM[\"Nouvelle Triangulation Francaise (Paris)\",\n"
        "      ELLIPSOID[\"Clarke 1880 "
        "(IGN)\",6378249.2,293.4660213,LENGTHUNIT[\"metre\",1.0]]],\n"
        "    PRIMEM[\"Paris\",2.5969213,ANGLEUNIT[\"grad\",0.015707963268]]],\n"
        "  CONVERSION[\"Lambert Nord France\",\n"
        "    METHOD[\"Lambert Conic Conformal (1SP)\",ID[\"EPSG\",9801]],\n"
        "    PARAMETER[\"Latitude of natural "
        "origin\",55,ANGLEUNIT[\"grad\",0.015707963268]],\n"
        "    PARAMETER[\"Longitude of natural "
        "origin\",0,ANGLEUNIT[\"grad\",0.015707963268]],\n"
        "    PARAMETER[\"Scale factor at natural "
        "origin\",0.999877341,SCALEUNIT[\"unity\",1.0]],\n"
        "    PARAMETER[\"False easting\",600000,LENGTHUNIT[\"metre\",1.0]],\n"
        "    PARAMETER[\"False northing\",200000,LENGTHUNIT[\"metre\",1.0]]],\n"
        "  CS[cartesian,2],\n"
        "    AXIS[\"easting (X)\",east,ORDER[1]],\n"
        "    AXIS[\"northing (Y)\",north,ORDER[2]],\n"
        "    LENGTHUNIT[\"metre\",1.0],\n"
        "  ID[\"EPSG\",27561]]");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->baseCRS()->coordinateSystem()->axisList()[0]->unit(),
              UnitOfMeasure::GRAD);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, cs_with_MERIDIAN) {
    auto wkt =
        "PROJCRS[\"dummy\",\n"
        "    BASEGEOGCRS[\"WGS 84\",\n"
        "        DATUM[\"World Geodetic System 1984\",\n"
        "            ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
        "        UNIT[\"degree\",0.0174532925199433]],\n"
        "    CONVERSION[\"dummy\",\n"
        "        METHOD[\"dummy\"],\n"
        "        PARAMETER[\"dummy\",1.0]],\n"
        "    CS[Cartesian,2],\n"
        "        AXIS[\"easting "
        "(X)\",south,MERIDIAN[90,ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "        AXIS[\"northing (Y)\",north],\n"
        "        UNIT[\"metre\",1],\n"
        "    ID[\"EPSG\",32631]]";
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    ASSERT_EQ(crs->coordinateSystem()->axisList().size(), 2);
    auto axis = crs->coordinateSystem()->axisList()[0];
    auto meridian = axis->meridian();
    ASSERT_TRUE(meridian != nullptr);
    EXPECT_EQ(meridian->longitude().value(), 90.0);
    EXPECT_EQ(meridian->longitude().unit(), UnitOfMeasure::DEGREE);
    ASSERT_TRUE(crs->coordinateSystem()->axisList()[1]->meridian() == nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, cs_with_multiple_ID) {
    auto wkt = "GEODCRS[\"WGS 84\",\n"
               "    DATUM[\"World Geodetic System 1984\",\n"
               "        ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
               "    CS[Cartesian,3],\n"
               "        AXIS[\"(X)\",geocentricX],\n"
               "        AXIS[\"(Y)\",geocentricY],\n"
               "        AXIS[\"(Z)\",geocentricZ],\n"
               "        UNIT[\"metre\",1],\n"
               "    ID[\"authorityA\",\"codeA\"],\n"
               "    ID[\"authorityB\",\"codeB\"]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->nameStr(), "WGS 84");
    ASSERT_EQ(crs->identifiers().size(), 2);
    EXPECT_EQ(crs->identifiers()[0]->code(), "codeA");
    EXPECT_EQ(*(crs->identifiers()[0]->codeSpace()), "authorityA");
    EXPECT_EQ(crs->identifiers()[1]->code(), "codeB");
    EXPECT_EQ(*(crs->identifiers()[1]->codeSpace()), "authorityB");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, vertcrs_WKT2) {
    auto wkt = "VERTCRS[\"ODN height\",\n"
               "    VDATUM[\"Ordnance Datum Newlyn\"],\n"
               "    CS[vertical,1],\n"
               "        AXIS[\"gravity-related height (H)\",up,\n"
               "            LENGTHUNIT[\"metre\",1]],\n"
               "    ID[\"EPSG\",5701]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<VerticalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->nameStr(), "ODN height");
    ASSERT_EQ(crs->identifiers().size(), 1);
    EXPECT_EQ(crs->identifiers()[0]->code(), "5701");
    EXPECT_EQ(*(crs->identifiers()[0]->codeSpace()), "EPSG");

    auto datum = crs->datum();
    EXPECT_EQ(datum->nameStr(), "Ordnance Datum Newlyn");
    // ASSERT_EQ(datum->identifiers().size(), 1);
    // EXPECT_EQ(datum->identifiers()[0]->code(), "5101");
    // EXPECT_EQ(*(datum->identifiers()[0]->codeSpace()), "EPSG");

    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 1);
    EXPECT_EQ(cs->axisList()[0]->nameStr(), "Gravity-related height");
    EXPECT_EQ(cs->axisList()[0]->abbreviation(), "H");
    EXPECT_EQ(cs->axisList()[0]->direction(), AxisDirection::UP);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, vertcrs_VRF_WKT2) {
    auto wkt = "VERTCRS[\"ODN height\",\n"
               "    VRF[\"Ordnance Datum Newlyn\"],\n"
               "    CS[vertical,1],\n"
               "        AXIS[\"gravity-related height (H)\",up,\n"
               "            LENGTHUNIT[\"metre\",1]],\n"
               "    ID[\"EPSG\",5701]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<VerticalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, vertcrs_with_GEOIDMODEL) {
    auto wkt = "VERTCRS[\"CGVD2013\","
               "    VRF[\"Canadian Geodetic Vertical Datum of 2013\"],"
               "    CS[vertical,1],"
               "        AXIS[\"gravity-related height (H)\",up],"
               "        LENGTHUNIT[\"metre\",1.0],"
               "    GEOIDMODEL[\"CGG2013\",ID[\"EPSG\",6648]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<VerticalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, vertcrs_WKT1_GDAL) {
    auto wkt = "VERT_CS[\"ODN height\",\n"
               "    VERT_DATUM[\"Ordnance Datum Newlyn\",2005,\n"
               "        AUTHORITY[\"EPSG\",\"5101\"]],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]],\n"
               "    AXIS[\"gravity-related height\",UP],\n"
               "    AUTHORITY[\"EPSG\",\"5701\"]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<VerticalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->nameStr(), "ODN height");
    ASSERT_EQ(crs->identifiers().size(), 1);
    EXPECT_EQ(crs->identifiers()[0]->code(), "5701");
    EXPECT_EQ(*(crs->identifiers()[0]->codeSpace()), "EPSG");

    auto datum = crs->datum();
    EXPECT_EQ(datum->nameStr(), "Ordnance Datum Newlyn");
    ASSERT_EQ(datum->identifiers().size(), 1);
    EXPECT_EQ(datum->identifiers()[0]->code(), "5101");
    EXPECT_EQ(*(datum->identifiers()[0]->codeSpace()), "EPSG");

    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 1);
    EXPECT_EQ(cs->axisList()[0]->nameStr(), "Gravity-related height");
    EXPECT_EQ(cs->axisList()[0]->abbreviation(), ""); // "H" in WKT2
    EXPECT_EQ(cs->axisList()[0]->direction(), AxisDirection::UP);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, vertcrs_WKT1_GDAL_minimum) {
    auto wkt = "VERT_CS[\"ODN height\",\n"
               "    VERT_DATUM[\"Ordnance Datum Newlyn\",2005],\n"
               "    UNIT[\"metre\",1]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<VerticalCRS>(obj);
    EXPECT_EQ(crs->nameStr(), "ODN height");

    auto datum = crs->datum();
    EXPECT_EQ(datum->nameStr(), "Ordnance Datum Newlyn");

    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 1);
    EXPECT_EQ(cs->axisList()[0]->nameStr(), "Gravity-related height");
    EXPECT_EQ(cs->axisList()[0]->direction(), AxisDirection::UP);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, dynamic_vertical_reference_frame) {
    auto obj = WKTParser().createFromWKT(
        "VERTCRS[\"RH2000\","
        "  DYNAMIC[FRAMEEPOCH[2000.0],MODEL[\"NKG2016LU\"]],"
        "  VDATUM[\"Rikets Hojdsystem 2000\",ANCHOR[\"my anchor\"]],"
        "  CS[vertical,1],"
        "    AXIS[\"gravity-related height (H)\",up],"
        "    LENGTHUNIT[\"metre\",1.0]"
        "]");
    auto crs = nn_dynamic_pointer_cast<VerticalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    auto dgrf =
        std::dynamic_pointer_cast<DynamicVerticalReferenceFrame>(crs->datum());
    ASSERT_TRUE(dgrf != nullptr);
    auto anchor = dgrf->anchorDefinition();
    EXPECT_TRUE(anchor.has_value());
    EXPECT_EQ(*anchor, "my anchor");
    EXPECT_TRUE(dgrf->frameReferenceEpoch() ==
                Measure(2000.0, UnitOfMeasure::YEAR));
    auto model = dgrf->deformationModelName();
    EXPECT_TRUE(model.has_value());
    EXPECT_EQ(*model, "NKG2016LU");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, vertcrs_with_ensemble) {
    auto obj = WKTParser().createFromWKT(
        "VERTCRS[\"unnamed\",\n"
        "    ENSEMBLE[\"unnamed\",\n"
        "        MEMBER[\"vdatum1\"],\n"
        "        MEMBER[\"vdatum2\"],\n"
        "        ENSEMBLEACCURACY[100]],\n"
        "    CS[vertical,1],\n"
        "        AXIS[\"gravity-related height (H)\",up,\n"
        "            LENGTHUNIT[\"metre\",1]]]");
    auto crs = nn_dynamic_pointer_cast<VerticalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    ASSERT_TRUE(crs->datum() == nullptr);
    ASSERT_TRUE(crs->datumEnsemble() != nullptr);
    EXPECT_EQ(crs->datumEnsemble()->datums().size(), 2);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, vdatum_with_ANCHOR) {
    auto obj = WKTParser().createFromWKT("VDATUM[\"Ordnance Datum Newlyn\",\n"
                                         "    ANCHOR[\"my anchor\"],\n"
                                         "    ID[\"EPSG\",5101]]");
    auto datum = nn_dynamic_pointer_cast<VerticalReferenceFrame>(obj);
    ASSERT_TRUE(datum != nullptr);
    auto anchor = datum->anchorDefinition();
    EXPECT_TRUE(anchor.has_value());
    EXPECT_EQ(*anchor, "my anchor");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, COMPOUNDCRS) {
    auto obj = WKTParser().createFromWKT(
        "COMPOUNDCRS[\"horizontal + vertical\",\n"
        "    PROJCRS[\"WGS 84 / UTM zone 31N\",\n"
        "        BASEGEODCRS[\"WGS 84\",\n"
        "            DATUM[\"World Geodetic System 1984\",\n"
        "                ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                    LENGTHUNIT[\"metre\",1]]],\n"
        "            PRIMEM[\"Greenwich\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "        CONVERSION[\"UTM zone 31N\",\n"
        "            METHOD[\"Transverse Mercator\",\n"
        "                ID[\"EPSG\",9807]],\n"
        "            PARAMETER[\"Latitude of natural origin\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8801]],\n"
        "            PARAMETER[\"Longitude of natural origin\",3,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8802]],\n"
        "            PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                SCALEUNIT[\"unity\",1],\n"
        "                ID[\"EPSG\",8805]],\n"
        "            PARAMETER[\"False easting\",500000,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8806]],\n"
        "            PARAMETER[\"False northing\",0,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8807]]],\n"
        "        CS[Cartesian,2],\n"
        "            AXIS[\"(E)\",east,\n"
        "                ORDER[1],\n"
        "                LENGTHUNIT[\"metre\",1]],\n"
        "            AXIS[\"(N)\",north,\n"
        "                ORDER[2],\n"
        "                LENGTHUNIT[\"metre\",1]]],\n"
        "    VERTCRS[\"ODN height\",\n"
        "        VDATUM[\"Ordnance Datum Newlyn\"],\n"
        "        CS[vertical,1],\n"
        "            AXIS[\"gravity-related height (H)\",up,\n"
        "                LENGTHUNIT[\"metre\",1]]],\n"
        "    ID[\"codespace\",\"code\"]]");
    auto crs = nn_dynamic_pointer_cast<CompoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->nameStr(), "horizontal + vertical");
    EXPECT_EQ(crs->componentReferenceSystems().size(), 2);
    ASSERT_EQ(crs->identifiers().size(), 1);
    EXPECT_EQ(crs->identifiers()[0]->code(), "code");
    EXPECT_EQ(*(crs->identifiers()[0]->codeSpace()), "codespace");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, COMPOUNDCRS_spatio_parametric_2015) {
    auto obj = WKTParser().createFromWKT(
        "COMPOUNDCRS[\"ICAO layer 0\",\n"
        "    GEODETICCRS[\"WGS 84\",\n"
        "        DATUM[\"World Geodetic System 1984\",\n"
        "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                LENGTHUNIT[\"metre\",1]]],\n"
        "        PRIMEM[\"Greenwich\",0,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8901]],\n"
        "        CS[ellipsoidal,2],\n"
        "            AXIS[\"latitude\",north,\n"
        "                ORDER[1],\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "            AXIS[\"longitude\",east,\n"
        "                ORDER[2],\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "    PARAMETRICCRS[\"WMO standard atmosphere\",\n"
        "        PARAMETRICDATUM[\"Mean Sea Level\",\n"
        "            ANCHOR[\"Mean Sea Level = 1013.25 hPa\"]],\n"
        "        CS[parametric,1],\n"
        "            AXIS[\"pressure (P)\",unspecified,\n"
        "                PARAMETRICUNIT[\"HectoPascal\",100]]]]");
    auto crs = nn_dynamic_pointer_cast<CompoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, COMPOUNDCRS_spatio_parametric_2018) {
    auto obj = WKTParser().createFromWKT(
        "COMPOUNDCRS[\"ICAO layer 0\",\n"
        "    GEOGRAPHICCRS[\"WGS 84\",\n"
        "    DYNAMIC[FRAMEEPOCH[2005]],\n"
        "        DATUM[\"World Geodetic System 1984\",\n"
        "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                LENGTHUNIT[\"metre\",1]]],\n"
        "        PRIMEM[\"Greenwich\",0,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8901]],\n"
        "        CS[ellipsoidal,2],\n"
        "            AXIS[\"latitude\",north,\n"
        "                ORDER[1],\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "            AXIS[\"longitude\",east,\n"
        "                ORDER[2],\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "    PARAMETRICCRS[\"WMO standard atmosphere\",\n"
        "        PARAMETRICDATUM[\"Mean Sea Level\",\n"
        "            ANCHOR[\"Mean Sea Level = 1013.25 hPa\"]],\n"
        "        CS[parametric,1],\n"
        "            AXIS[\"pressure (P)\",unspecified,\n"
        "                PARAMETRICUNIT[\"HectoPascal\",100]]]]");
    auto crs = nn_dynamic_pointer_cast<CompoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, COMPOUNDCRS_spatio_temporal_2015) {
    auto obj = WKTParser().createFromWKT(
        "COMPOUNDCRS[\"GPS position and time\",\n"
        "    GEODCRS[\"WGS 84 (G1762)\",\n"
        "        DATUM[\"World Geodetic System 1984 (G1762)\",\n"
        "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                LENGTHUNIT[\"metre\",1,\n"
        "                    ID[\"EPSG\",9001]]]],\n"
        "        CS[ellipsoidal,2],\n"
        "            AXIS[\"latitude\",north,\n"
        "                ORDER[1],\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "            AXIS[\"longitude\",east,\n"
        "                ORDER[2],\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "    TIMECRS[\"GPS Time\",\n"
        "        TIMEDATUM[\"Time origin\",TIMEORIGIN[1980-01-01]],\n"
        "        CS[temporal,1],\n"
        "            AXIS[\"time (T)\",future]]]");
    auto crs = nn_dynamic_pointer_cast<CompoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, COMPOUNDCRS_spatio_temporal_2018) {
    auto obj = WKTParser().createFromWKT(
        "COMPOUNDCRS[\"2D GPS position with civil time in ISO 8601 format\",\n"
        "    GEOGCRS[\"WGS 84 (G1762)\",\n"
        "        DATUM[\"World Geodetic System 1984 (G1762)\",\n"
        "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                LENGTHUNIT[\"metre\",1,\n"
        "                    ID[\"EPSG\",9001]]]],\n"
        "        CS[ellipsoidal,2],\n"
        "            AXIS[\"latitude\",north,\n"
        "                ORDER[1],\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "            AXIS[\"longitude\",east,\n"
        "                ORDER[2],\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "    TIMECRS[\"DateTime\",\n"
        "        TDATUM[\"Gregorian Calendar\"],\n"
        "        CS[TemporalDateTime,1],\n"
        "            AXIS[\"time (T)\",future]]]");
    auto crs = nn_dynamic_pointer_cast<CompoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, COMPD_CS) {
    auto obj = WKTParser().createFromWKT(
        "COMPD_CS[\"horizontal + vertical\",\n"
        "    PROJCS[\"WGS 84 / UTM zone 31N\",\n"
        "        GEOGCS[\"WGS 84\",\n"
        "            DATUM[\"World Geodetic System 1984\",\n"
        "                SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
        "                    AUTHORITY[\"EPSG\",\"7030\"]],\n"
        "                AUTHORITY[\"EPSG\",\"6326\"]],\n"
        "            PRIMEM[\"Greenwich\",0,\n"
        "                AUTHORITY[\"EPSG\",\"8901\"]],\n"
        "            UNIT[\"degree\",0.0174532925199433,\n"
        "                AUTHORITY[\"EPSG\",\"9122\"]],\n"
        "            AXIS[\"Latitude\",NORTH],\n"
        "            AXIS[\"Longitude\",EAST],\n"
        "            AUTHORITY[\"EPSG\",\"4326\"]],\n"
        "        PROJECTION[\"Transverse_Mercator\"],\n"
        "        PARAMETER[\"latitude_of_origin\",0],\n"
        "        PARAMETER[\"central_meridian\",3],\n"
        "        PARAMETER[\"scale_factor\",0.9996],\n"
        "        PARAMETER[\"false_easting\",500000],\n"
        "        PARAMETER[\"false_northing\",0],\n"
        "        UNIT[\"metre\",1,\n"
        "            AUTHORITY[\"EPSG\",\"9001\"]],\n"
        "        AXIS[\"Easting\",EAST],\n"
        "        AXIS[\"Northing\",NORTH],\n"
        "        AUTHORITY[\"EPSG\",\"32631\"]],\n"
        "    VERT_CS[\"ODN height\",\n"
        "        VERT_DATUM[\"Ordnance Datum Newlyn\",2005,\n"
        "            AUTHORITY[\"EPSG\",\"5101\"]],\n"
        "        UNIT[\"metre\",1,\n"
        "            AUTHORITY[\"EPSG\",\"9001\"]],\n"
        "        AXIS[\"Gravity-related height\",UP],\n"
        "        AUTHORITY[\"EPSG\",\"5701\"]],\n"
        "    AUTHORITY[\"codespace\",\"code\"]]");
    auto crs = nn_dynamic_pointer_cast<CompoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->nameStr(), "horizontal + vertical");
    EXPECT_EQ(crs->componentReferenceSystems().size(), 2);
    ASSERT_EQ(crs->identifiers().size(), 1);
    EXPECT_EQ(crs->identifiers()[0]->code(), "code");
    EXPECT_EQ(*(crs->identifiers()[0]->codeSpace()), "codespace");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, COORDINATEOPERATION) {

    std::string src_wkt;
    {
        auto formatter = WKTFormatter::create();
        formatter->setOutputId(false);
        src_wkt = GeographicCRS::EPSG_4326->exportToWKT(formatter.get());
    }

    std::string dst_wkt;
    {
        auto formatter = WKTFormatter::create();
        formatter->setOutputId(false);
        dst_wkt = GeographicCRS::EPSG_4807->exportToWKT(formatter.get());
    }

    std::string interpolation_wkt;
    {
        auto formatter = WKTFormatter::create();
        formatter->setOutputId(false);
        interpolation_wkt =
            GeographicCRS::EPSG_4979->exportToWKT(formatter.get());
    }

    auto wkt =
        "COORDINATEOPERATION[\"transformationName\",\n"
        "    SOURCECRS[" +
        src_wkt + "],\n"
                  "    TARGETCRS[" +
        dst_wkt +
        "],\n"
        "    METHOD[\"operationMethodName\",\n"
        "        ID[\"codeSpaceOperationMethod\",\"codeOperationMethod\"]],\n"
        "    PARAMETERFILE[\"paramName\",\"foo.bin\"],\n"
        "    INTERPOLATIONCRS[" +
        interpolation_wkt +
        "],\n"
        "    OPERATIONACCURACY[0.1],\n"
        "    ID[\"codeSpaceTransformation\",\"codeTransformation\"],\n"
        "    REMARK[\"my remarks\"]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(transf->nameStr(), "transformationName");
    ASSERT_EQ(transf->identifiers().size(), 1);
    EXPECT_EQ(transf->identifiers()[0]->code(), "codeTransformation");
    EXPECT_EQ(*(transf->identifiers()[0]->codeSpace()),
              "codeSpaceTransformation");
    ASSERT_EQ(transf->coordinateOperationAccuracies().size(), 1);
    EXPECT_EQ(transf->coordinateOperationAccuracies()[0]->value(), "0.1");
    EXPECT_EQ(transf->sourceCRS()->nameStr(),
              GeographicCRS::EPSG_4326->nameStr());
    EXPECT_EQ(transf->targetCRS()->nameStr(),
              GeographicCRS::EPSG_4807->nameStr());
    ASSERT_TRUE(transf->interpolationCRS() != nullptr);
    EXPECT_EQ(transf->interpolationCRS()->nameStr(),
              GeographicCRS::EPSG_4979->nameStr());
    EXPECT_EQ(transf->method()->nameStr(), "operationMethodName");
    EXPECT_EQ(transf->parameterValues().size(), 1);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, CONCATENATEDOPERATION) {

    auto transf_1 = Transformation::create(
        PropertyMap().set(IdentifiedObject::NAME_KEY, "transf_1"),
        nn_static_pointer_cast<CRS>(GeographicCRS::EPSG_4326),
        nn_static_pointer_cast<CRS>(GeographicCRS::EPSG_4807), nullptr,
        PropertyMap().set(IdentifiedObject::NAME_KEY, "operationMethodName"),
        std::vector<OperationParameterNNPtr>{OperationParameter::create(
            PropertyMap().set(IdentifiedObject::NAME_KEY, "paramName"))},
        std::vector<ParameterValueNNPtr>{
            ParameterValue::createFilename("foo.bin")},
        std::vector<PositionalAccuracyNNPtr>());

    auto transf_2 = Transformation::create(
        PropertyMap().set(IdentifiedObject::NAME_KEY, "transf_2"),
        nn_static_pointer_cast<CRS>(GeographicCRS::EPSG_4807),
        nn_static_pointer_cast<CRS>(GeographicCRS::EPSG_4979), nullptr,
        PropertyMap().set(IdentifiedObject::NAME_KEY, "operationMethodName"),
        std::vector<OperationParameterNNPtr>{OperationParameter::create(
            PropertyMap().set(IdentifiedObject::NAME_KEY, "paramName"))},
        std::vector<ParameterValueNNPtr>{
            ParameterValue::createFilename("foo.bin")},
        std::vector<PositionalAccuracyNNPtr>());

    auto concat_in = ConcatenatedOperation::create(
        PropertyMap()
            .set(Identifier::CODESPACE_KEY, "codeSpace")
            .set(Identifier::CODE_KEY, "code")
            .set(IdentifiedObject::NAME_KEY, "name")
            .set(IdentifiedObject::REMARKS_KEY, "my remarks"),
        std::vector<CoordinateOperationNNPtr>{transf_1, transf_2},
        std::vector<PositionalAccuracyNNPtr>{
            PositionalAccuracy::create("0.1")});

    auto wkt = concat_in->exportToWKT(
        WKTFormatter::create(WKTFormatter::Convention::WKT2_2018).get());

    auto obj = WKTParser().createFromWKT(wkt);
    auto concat = nn_dynamic_pointer_cast<ConcatenatedOperation>(obj);
    ASSERT_TRUE(concat != nullptr);
    EXPECT_EQ(concat->nameStr(), "name");
    ASSERT_EQ(concat->identifiers().size(), 1);
    EXPECT_EQ(concat->identifiers()[0]->code(), "code");
    EXPECT_EQ(*(concat->identifiers()[0]->codeSpace()), "codeSpace");
    ASSERT_EQ(concat->operations().size(), 2);
    ASSERT_EQ(concat->operations()[0]->nameStr(), transf_1->nameStr());
    ASSERT_EQ(concat->operations()[1]->nameStr(), transf_2->nameStr());
    ASSERT_TRUE(concat->sourceCRS() != nullptr);
    ASSERT_TRUE(concat->targetCRS() != nullptr);
    ASSERT_EQ(concat->sourceCRS()->nameStr(), transf_1->sourceCRS()->nameStr());
    ASSERT_EQ(concat->targetCRS()->nameStr(), transf_2->targetCRS()->nameStr());
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, CONCATENATEDOPERATION_with_conversion_and_conversion) {

    auto wkt =
        "CONCATENATEDOPERATION[\"Inverse of UTM zone 31N + UTM zone 32N\",\n"
        "    SOURCECRS[\n"
        "        PROJCRS[\"WGS 84 / UTM zone 31N\",\n"
        "            BASEGEOGCRS[\"WGS 84\",\n"
        "                DATUM[\"World Geodetic System 1984\",\n"
        "                    ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                        LENGTHUNIT[\"metre\",1]]],\n"
        "                PRIMEM[\"Greenwich\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "            CONVERSION[\"UTM zone 31N\",\n"
        "                METHOD[\"Transverse Mercator\",\n"
        "                    ID[\"EPSG\",9807]],\n"
        "                PARAMETER[\"Latitude of natural origin\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8801]],\n"
        "                PARAMETER[\"Longitude of natural origin\",3,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8802]],\n"
        "                PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                    SCALEUNIT[\"unity\",1],\n"
        "                    ID[\"EPSG\",8805]],\n"
        "                PARAMETER[\"False easting\",500000,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8806]],\n"
        "                PARAMETER[\"False northing\",0,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8807]]],\n"
        "            CS[Cartesian,2],\n"
        "                AXIS[\"(E)\",east,\n"
        "                    ORDER[1],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "                AXIS[\"(N)\",north,\n"
        "                    ORDER[2],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "            ID[\"EPSG\",32631]]],\n"
        "    TARGETCRS[\n"
        "        PROJCRS[\"WGS 84 / UTM zone 32N\",\n"
        "            BASEGEOGCRS[\"WGS 84\",\n"
        "                DATUM[\"World Geodetic System 1984\",\n"
        "                    ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                        LENGTHUNIT[\"metre\",1]]],\n"
        "                PRIMEM[\"Greenwich\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "            CONVERSION[\"UTM zone 32N\",\n"
        "                METHOD[\"Transverse Mercator\",\n"
        "                    ID[\"EPSG\",9807]],\n"
        "                PARAMETER[\"Latitude of natural origin\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8801]],\n"
        "                PARAMETER[\"Longitude of natural origin\",9,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8802]],\n"
        "                PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                    SCALEUNIT[\"unity\",1],\n"
        "                    ID[\"EPSG\",8805]],\n"
        "                PARAMETER[\"False easting\",500000,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8806]],\n"
        "                PARAMETER[\"False northing\",0,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8807]]],\n"
        "            CS[Cartesian,2],\n"
        "                AXIS[\"(E)\",east,\n"
        "                    ORDER[1],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "                AXIS[\"(N)\",north,\n"
        "                    ORDER[2],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "            ID[\"EPSG\",32632]]],\n"
        "    STEP[\n"
        "        CONVERSION[\"Inverse of UTM zone 31N\",\n"
        "            METHOD[\"Inverse of Transverse Mercator\",\n"
        "                ID[\"INVERSE(EPSG)\",9807]],\n"
        "            PARAMETER[\"Latitude of natural origin\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8801]],\n"
        "            PARAMETER[\"Longitude of natural origin\",3,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8802]],\n"
        "            PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                SCALEUNIT[\"unity\",1],\n"
        "                ID[\"EPSG\",8805]],\n"
        "            PARAMETER[\"False easting\",500000,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8806]],\n"
        "            PARAMETER[\"False northing\",0,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8807]],\n"
        "            ID[\"INVERSE(EPSG)\",16031]]],\n"
        "    STEP[\n"
        "        CONVERSION[\"UTM zone 32N\",\n"
        "            METHOD[\"Transverse Mercator\",\n"
        "                ID[\"EPSG\",9807]],\n"
        "            PARAMETER[\"Latitude of natural origin\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8801]],\n"
        "            PARAMETER[\"Longitude of natural origin\",9,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8802]],\n"
        "            PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                SCALEUNIT[\"unity\",1],\n"
        "                ID[\"EPSG\",8805]],\n"
        "            PARAMETER[\"False easting\",500000,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8806]],\n"
        "            PARAMETER[\"False northing\",0,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8807]],\n"
        "            ID[\"EPSG\",16032]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto concat = nn_dynamic_pointer_cast<ConcatenatedOperation>(obj);
    ASSERT_TRUE(concat != nullptr);

    EXPECT_EQ(concat->exportToPROJString(PROJStringFormatter::create().get()),
              "+proj=pipeline +step +inv +proj=utm +zone=31 +ellps=WGS84 "
              "+step +proj=utm +zone=32 +ellps=WGS84");

    auto outWkt = concat->exportToWKT(
        WKTFormatter::create(WKTFormatter::Convention::WKT2_2018).get());
    EXPECT_EQ(wkt, outWkt);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse,
     CONCATENATEDOPERATION_with_conversion_coordinateoperation_conversion) {

    auto wkt =
        "CONCATENATEDOPERATION[\"Inverse of UTM zone 11N + NAD27 to WGS 84 "
        "(79) + UTM zone 11N\",\n"
        "    SOURCECRS[\n"
        "        PROJCRS[\"NAD27 / UTM zone 11N\",\n"
        "            BASEGEOGCRS[\"NAD27\",\n"
        "                DATUM[\"North American Datum 1927\",\n"
        "                    ELLIPSOID[\"Clarke "
        "1866\",6378206.4,294.978698213898,\n"
        "                        LENGTHUNIT[\"metre\",1]]],\n"
        "                PRIMEM[\"Greenwich\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "            CONVERSION[\"UTM zone 11N\",\n"
        "                METHOD[\"Transverse Mercator\",\n"
        "                    ID[\"EPSG\",9807]],\n"
        "                PARAMETER[\"Latitude of natural origin\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8801]],\n"
        "                PARAMETER[\"Longitude of natural origin\",-117,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8802]],\n"
        "                PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                    SCALEUNIT[\"unity\",1],\n"
        "                    ID[\"EPSG\",8805]],\n"
        "                PARAMETER[\"False easting\",500000,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8806]],\n"
        "                PARAMETER[\"False northing\",0,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8807]]],\n"
        "            CS[Cartesian,2],\n"
        "                AXIS[\"(E)\",east,\n"
        "                    ORDER[1],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "                AXIS[\"(N)\",north,\n"
        "                    ORDER[2],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "            ID[\"EPSG\",26711]]],\n"
        "    TARGETCRS[\n"
        "        PROJCRS[\"WGS 84 / UTM zone 11N\",\n"
        "            BASEGEOGCRS[\"WGS 84\",\n"
        "                DATUM[\"World Geodetic System 1984\",\n"
        "                    ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                        LENGTHUNIT[\"metre\",1]]],\n"
        "                PRIMEM[\"Greenwich\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "            CONVERSION[\"UTM zone 11N\",\n"
        "                METHOD[\"Transverse Mercator\",\n"
        "                    ID[\"EPSG\",9807]],\n"
        "                PARAMETER[\"Latitude of natural origin\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8801]],\n"
        "                PARAMETER[\"Longitude of natural origin\",-117,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8802]],\n"
        "                PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                    SCALEUNIT[\"unity\",1],\n"
        "                    ID[\"EPSG\",8805]],\n"
        "                PARAMETER[\"False easting\",500000,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8806]],\n"
        "                PARAMETER[\"False northing\",0,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8807]]],\n"
        "            CS[Cartesian,2],\n"
        "                AXIS[\"(E)\",east,\n"
        "                    ORDER[1],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "                AXIS[\"(N)\",north,\n"
        "                    ORDER[2],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "            ID[\"EPSG\",32611]]],\n"
        "    STEP[\n"
        "        CONVERSION[\"Inverse of UTM zone 11N\",\n"
        "            METHOD[\"Inverse of Transverse Mercator\",\n"
        "                ID[\"INVERSE(EPSG)\",9807]],\n"
        "            PARAMETER[\"Latitude of natural origin\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8801]],\n"
        "            PARAMETER[\"Longitude of natural origin\",-117,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8802]],\n"
        "            PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                SCALEUNIT[\"unity\",1],\n"
        "                ID[\"EPSG\",8805]],\n"
        "            PARAMETER[\"False easting\",500000,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8806]],\n"
        "            PARAMETER[\"False northing\",0,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8807]],\n"
        "            ID[\"INVERSE(EPSG)\",16011]]],\n"
        "    STEP[\n"
        "        COORDINATEOPERATION[\"NAD27 to WGS 84 (79)\",\n"
        "            SOURCECRS[\n"
        "                GEOGCRS[\"NAD27\",\n"
        "                    DATUM[\"North American Datum 1927\",\n"
        "                        ELLIPSOID[\"Clarke "
        "1866\",6378206.4,294.978698213898,\n"
        "                            LENGTHUNIT[\"metre\",1]]],\n"
        "                    PRIMEM[\"Greenwich\",0,\n"
        "                        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "                    CS[ellipsoidal,2],\n"
        "                        AXIS[\"geodetic latitude (Lat)\",north,\n"
        "                            ORDER[1],\n"
        "                            "
        "ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "                        AXIS[\"geodetic longitude (Lon)\",east,\n"
        "                            ORDER[2],\n"
        "                            "
        "ANGLEUNIT[\"degree\",0.0174532925199433]]]],\n"
        "            TARGETCRS[\n"
        "                GEOGCRS[\"WGS 84\",\n"
        "                    DATUM[\"World Geodetic System 1984\",\n"
        "                        ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                            LENGTHUNIT[\"metre\",1]]],\n"
        "                    PRIMEM[\"Greenwich\",0,\n"
        "                        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "                    CS[ellipsoidal,2],\n"
        "                        AXIS[\"geodetic latitude (Lat)\",north,\n"
        "                            ORDER[1],\n"
        "                            "
        "ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "                        AXIS[\"geodetic longitude (Lon)\",east,\n"
        "                            ORDER[2],\n"
        "                            "
        "ANGLEUNIT[\"degree\",0.0174532925199433]]]],\n"
        "            METHOD[\"CTABLE2\"],\n"
        "            PARAMETERFILE[\"Latitude and longitude difference "
        "file\",\"conus\"],\n"
        "            ID[\"DERIVED_FROM(EPSG)\",15851]]],\n"
        "    STEP[\n"
        "        CONVERSION[\"UTM zone 11N\",\n"
        "            METHOD[\"Transverse Mercator\",\n"
        "                ID[\"EPSG\",9807]],\n"
        "            PARAMETER[\"Latitude of natural origin\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8801]],\n"
        "            PARAMETER[\"Longitude of natural origin\",-117,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8802]],\n"
        "            PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                SCALEUNIT[\"unity\",1],\n"
        "                ID[\"EPSG\",8805]],\n"
        "            PARAMETER[\"False easting\",500000,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8806]],\n"
        "            PARAMETER[\"False northing\",0,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8807]],\n"
        "            ID[\"EPSG\",16011]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto concat = nn_dynamic_pointer_cast<ConcatenatedOperation>(obj);
    ASSERT_TRUE(concat != nullptr);

    EXPECT_EQ(concat->exportToPROJString(PROJStringFormatter::create().get()),
              "+proj=pipeline +step +inv +proj=utm +zone=11 +ellps=clrk66 "
              "+step +proj=hgridshift +grids=conus +step +proj=utm "
              "+zone=11 +ellps=WGS84");

    auto outWkt = concat->exportToWKT(
        WKTFormatter::create(WKTFormatter::Convention::WKT2_2018).get());
    EXPECT_EQ(wkt, outWkt);
}

// ---------------------------------------------------------------------------

TEST(
    wkt_parse,
    CONCATENATEDOPERATION_with_conversion_coordinateoperation_to_inverse_conversion) {

    auto wkt =
        "CONCATENATEDOPERATION[\"Inverse of UTM zone 11N + NAD27 to WGS 84 "
        "(79) + UTM zone 11N\",\n"
        "    SOURCECRS[\n"
        "        PROJCRS[\"WGS 84 / UTM zone 11N\",\n"
        "            BASEGEOGCRS[\"WGS 84\",\n"
        "                DATUM[\"World Geodetic System 1984\",\n"
        "                    ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                        LENGTHUNIT[\"metre\",1]]],\n"
        "                PRIMEM[\"Greenwich\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "            CONVERSION[\"UTM zone 11N\",\n"
        "                METHOD[\"Transverse Mercator\",\n"
        "                    ID[\"EPSG\",9807]],\n"
        "                PARAMETER[\"Latitude of natural origin\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8801]],\n"
        "                PARAMETER[\"Longitude of natural origin\",-117,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8802]],\n"
        "                PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                    SCALEUNIT[\"unity\",1],\n"
        "                    ID[\"EPSG\",8805]],\n"
        "                PARAMETER[\"False easting\",500000,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8806]],\n"
        "                PARAMETER[\"False northing\",0,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8807]]],\n"
        "            CS[Cartesian,2],\n"
        "                AXIS[\"(E)\",east,\n"
        "                    ORDER[1],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "                AXIS[\"(N)\",north,\n"
        "                    ORDER[2],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "            ID[\"EPSG\",32611]]],\n"
        "    TARGETCRS[\n"
        "        PROJCRS[\"NAD27 / UTM zone 11N\",\n"
        "            BASEGEOGCRS[\"NAD27\",\n"
        "                DATUM[\"North American Datum 1927\",\n"
        "                    ELLIPSOID[\"Clarke "
        "1866\",6378206.4,294.978698213898,\n"
        "                        LENGTHUNIT[\"metre\",1]]],\n"
        "                PRIMEM[\"Greenwich\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "            CONVERSION[\"UTM zone 11N\",\n"
        "                METHOD[\"Transverse Mercator\",\n"
        "                    ID[\"EPSG\",9807]],\n"
        "                PARAMETER[\"Latitude of natural origin\",0,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8801]],\n"
        "                PARAMETER[\"Longitude of natural origin\",-117,\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                    ID[\"EPSG\",8802]],\n"
        "                PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                    SCALEUNIT[\"unity\",1],\n"
        "                    ID[\"EPSG\",8805]],\n"
        "                PARAMETER[\"False easting\",500000,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8806]],\n"
        "                PARAMETER[\"False northing\",0,\n"
        "                    LENGTHUNIT[\"metre\",1],\n"
        "                    ID[\"EPSG\",8807]]],\n"
        "            CS[Cartesian,2],\n"
        "                AXIS[\"(E)\",east,\n"
        "                    ORDER[1],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "                AXIS[\"(N)\",north,\n"
        "                    ORDER[2],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "            ID[\"EPSG\",26711]]],\n"
        "    STEP[\n"
        "        CONVERSION[\"Inverse of UTM zone 11N\",\n"
        "            METHOD[\"Inverse of Transverse Mercator\",\n"
        "                ID[\"INVERSE(EPSG)\",9807]],\n"
        "            PARAMETER[\"Latitude of natural origin\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8801]],\n"
        "            PARAMETER[\"Longitude of natural origin\",-117,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8802]],\n"
        "            PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                SCALEUNIT[\"unity\",1],\n"
        "                ID[\"EPSG\",8805]],\n"
        "            PARAMETER[\"False easting\",500000,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8806]],\n"
        "            PARAMETER[\"False northing\",0,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8807]],\n"
        "            ID[\"INVERSE(EPSG)\",16011]]],\n"
        "    STEP[\n"
        "        COORDINATEOPERATION[\"NAD27 to WGS 84 (79)\",\n"
        "            SOURCECRS[\n"
        "                GEOGCRS[\"NAD27\",\n"
        "                    DATUM[\"North American Datum 1927\",\n"
        "                        ELLIPSOID[\"Clarke "
        "1866\",6378206.4,294.978698213898,\n"
        "                            LENGTHUNIT[\"metre\",1]]],\n"
        "                    PRIMEM[\"Greenwich\",0,\n"
        "                        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "                    CS[ellipsoidal,2],\n"
        "                        AXIS[\"geodetic latitude (Lat)\",north,\n"
        "                            ORDER[1],\n"
        "                            "
        "ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "                        AXIS[\"geodetic longitude (Lon)\",east,\n"
        "                            ORDER[2],\n"
        "                            "
        "ANGLEUNIT[\"degree\",0.0174532925199433]]]],\n"
        "            TARGETCRS[\n"
        "                GEOGCRS[\"WGS 84\",\n"
        "                    DATUM[\"World Geodetic System 1984\",\n"
        "                        ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                            LENGTHUNIT[\"metre\",1]]],\n"
        "                    PRIMEM[\"Greenwich\",0,\n"
        "                        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "                    CS[ellipsoidal,2],\n"
        "                        AXIS[\"geodetic latitude (Lat)\",north,\n"
        "                            ORDER[1],\n"
        "                            "
        "ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "                        AXIS[\"geodetic longitude (Lon)\",east,\n"
        "                            ORDER[2],\n"
        "                            "
        "ANGLEUNIT[\"degree\",0.0174532925199433]]]],\n"
        "            METHOD[\"CTABLE2\"],\n"
        "            PARAMETERFILE[\"Latitude and longitude difference "
        "file\",\"conus\"],\n"
        "            ID[\"DERIVED_FROM(EPSG)\",15851]]],\n"
        "    STEP[\n"
        "        CONVERSION[\"UTM zone 11N\",\n"
        "            METHOD[\"Transverse Mercator\",\n"
        "                ID[\"EPSG\",9807]],\n"
        "            PARAMETER[\"Latitude of natural origin\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8801]],\n"
        "            PARAMETER[\"Longitude of natural origin\",-117,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8802]],\n"
        "            PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                SCALEUNIT[\"unity\",1],\n"
        "                ID[\"EPSG\",8805]],\n"
        "            PARAMETER[\"False easting\",500000,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8806]],\n"
        "            PARAMETER[\"False northing\",0,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8807]],\n"
        "            ID[\"EPSG\",16011]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto concat = nn_dynamic_pointer_cast<ConcatenatedOperation>(obj);
    ASSERT_TRUE(concat != nullptr);

    EXPECT_EQ(concat->exportToPROJString(PROJStringFormatter::create().get()),
              "+proj=pipeline +step +inv +proj=utm +zone=11 +ellps=WGS84 "
              "+step +inv +proj=hgridshift +grids=conus +step "
              "+proj=utm +zone=11 +ellps=clrk66");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, BOUNDCRS_transformation_from_names) {

    auto projcrs = ProjectedCRS::create(
        PropertyMap().set(IdentifiedObject::NAME_KEY, "my PROJCRS"),
        GeographicCRS::create(
            PropertyMap().set(IdentifiedObject::NAME_KEY, "my GEOGCRS"),
            GeodeticReferenceFrame::EPSG_6326,
            EllipsoidalCS::createLatitudeLongitude(UnitOfMeasure::DEGREE)),
        Conversion::createUTM(PropertyMap(), 31, true),
        CartesianCS::createEastingNorthing(UnitOfMeasure::METRE));

    auto wkt =
        "BOUNDCRS[SOURCECRS[" +
        projcrs->exportToWKT(WKTFormatter::create().get()) + "],\n" +
        "TARGETCRS[" +
        GeographicCRS::EPSG_4326->exportToWKT(WKTFormatter::create().get()) +
        "],\n"
        "    ABRIDGEDTRANSFORMATION[\"Transformation to WGS84\",\n"
        "        METHOD[\"Coordinate Frame\"],\n"
        "        PARAMETER[\"X-axis translation\",1],\n"
        "        PARAMETER[\"Y-axis translation\",2],\n"
        "        PARAMETER[\"Z-axis translation\",3],\n"
        "        PARAMETER[\"X-axis rotation\",-4],\n"
        "        PARAMETER[\"Y-axis rotation\",-5],\n"
        "        PARAMETER[\"Z-axis rotation\",-6],\n"
        "        PARAMETER[\"Scale difference\",1.000007]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->baseCRS()->nameStr(), projcrs->nameStr());

    EXPECT_EQ(crs->hubCRS()->nameStr(), GeographicCRS::EPSG_4326->nameStr());

    ASSERT_TRUE(crs->transformation()->sourceCRS() != nullptr);
    EXPECT_EQ(crs->transformation()->sourceCRS()->nameStr(),
              projcrs->baseCRS()->nameStr());

    auto params = crs->transformation()->getTOWGS84Parameters();
    auto expected = std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    ASSERT_EQ(params.size(), expected.size());
    for (int i = 0; i < 7; i++) {
        EXPECT_NEAR(params[i], expected[i], 1e-10);
    }
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, BOUNDCRS_transformation_from_codes) {

    auto projcrs = ProjectedCRS::create(
        PropertyMap().set(IdentifiedObject::NAME_KEY, "my PROJCRS"),
        GeographicCRS::create(
            PropertyMap().set(IdentifiedObject::NAME_KEY, "my GEOGCRS"),
            GeodeticReferenceFrame::EPSG_6326,
            EllipsoidalCS::createLatitudeLongitude(UnitOfMeasure::DEGREE)),
        Conversion::createUTM(PropertyMap(), 31, true),
        CartesianCS::createEastingNorthing(UnitOfMeasure::METRE));

    auto wkt =
        "BOUNDCRS[SOURCECRS[" +
        projcrs->exportToWKT(WKTFormatter::create().get()) + "],\n" +
        "TARGETCRS[" +
        GeographicCRS::EPSG_4326->exportToWKT(WKTFormatter::create().get()) +
        "],\n"
        "    ABRIDGEDTRANSFORMATION[\"Transformation to WGS84\",\n"
        "        METHOD[\"bla\",ID[\"EPSG\",1032]],\n"
        "        PARAMETER[\"tx\",1,ID[\"EPSG\",8605]],\n"
        "        PARAMETER[\"ty\",2,ID[\"EPSG\",8606]],\n"
        "        PARAMETER[\"tz\",3,ID[\"EPSG\",8607]],\n"
        "        PARAMETER[\"rotx\",-4,ID[\"EPSG\",8608]],\n"
        "        PARAMETER[\"roty\",-5,ID[\"EPSG\",8609]],\n"
        "        PARAMETER[\"rotz\",-6,ID[\"EPSG\",8610]],\n"
        "        PARAMETER[\"scale\",1.000007,ID[\"EPSG\",8611]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->baseCRS()->nameStr(), projcrs->nameStr());

    EXPECT_EQ(crs->hubCRS()->nameStr(), GeographicCRS::EPSG_4326->nameStr());

    ASSERT_TRUE(crs->transformation()->sourceCRS() != nullptr);
    EXPECT_EQ(crs->transformation()->sourceCRS()->nameStr(),
              projcrs->baseCRS()->nameStr());

    auto params = crs->transformation()->getTOWGS84Parameters();
    auto expected = std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    ASSERT_EQ(params.size(), expected.size());
    for (int i = 0; i < 7; i++) {
        EXPECT_NEAR(params[i], expected[i], 1e-10);
    }
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, boundcrs_of_verticalcrs_to_geog3Dcrs) {
    auto wkt =
        "BOUNDCRS[\n"
        "    SOURCECRS[\n"
        "        VERTCRS[\"my_height\",\n"
        "            VDATUM[\"my_height\"],\n"
        "            CS[vertical,1],\n"
        "                AXIS[\"up\",up,\n"
        "                    LENGTHUNIT[\"metre\",1,\n"
        "                        ID[\"EPSG\",9001]]]]],\n"
        "    TARGETCRS[\n"
        "        GEODCRS[\"WGS 84\",\n"
        "            DATUM[\"World Geodetic System 1984\",\n"
        "                ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                    LENGTHUNIT[\"metre\",1]]],\n"
        "            PRIMEM[\"Greenwich\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "            CS[ellipsoidal,3],\n"
        "                AXIS[\"latitude\",north,\n"
        "                    ORDER[1],\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "                AXIS[\"longitude\",east,\n"
        "                    ORDER[2],\n"
        "                    ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "                AXIS[\"ellipsoidal height\",up,\n"
        "                    ORDER[3],\n"
        "                    LENGTHUNIT[\"metre\",1]],\n"
        "            ID[\"EPSG\",4979]]],\n"
        "    ABRIDGEDTRANSFORMATION[\"my_height height to WGS84 ellipsoidal "
        "height\",\n"
        "        METHOD[\"GravityRelatedHeight to Geographic3D\"],\n"
        "        PARAMETERFILE[\"Geoid (height correction) model file\","
        "                      \"./tmp/fake.gtx\",\n"
        "            ID[\"EPSG\",8666]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->baseCRS()->nameStr(), "my_height");

    EXPECT_EQ(crs->hubCRS()->nameStr(), GeographicCRS::EPSG_4979->nameStr());
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, geogcs_TOWGS84_3terms) {
    auto wkt = "GEOGCS[\"my GEOGCRS\",\n"
               "    DATUM[\"WGS_1984\",\n"
               "        SPHEROID[\"WGS 84\",6378137,298.257223563],\n"
               "        TOWGS84[1,2,3]],\n"
               "    PRIMEM[\"Greenwich\",0],\n"
               "    UNIT[\"degree\",0.0174532925199433]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->baseCRS()->nameStr(), "my GEOGCRS");

    EXPECT_EQ(crs->hubCRS()->nameStr(), GeographicCRS::EPSG_4326->nameStr());

    ASSERT_TRUE(crs->transformation()->sourceCRS() != nullptr);
    EXPECT_EQ(crs->transformation()->sourceCRS()->nameStr(), "my GEOGCRS");

    auto params = crs->transformation()->getTOWGS84Parameters();
    auto expected = std::vector<double>{1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 0.0};
    ASSERT_EQ(params.size(), expected.size());
    for (int i = 0; i < 7; i++) {
        EXPECT_NEAR(params[i], expected[i], 1e-10);
    }
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, projcs_TOWGS84_7terms) {
    auto wkt = "PROJCS[\"my PROJCRS\",\n"
               "    GEOGCS[\"my GEOGCRS\",\n"
               "        DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            TOWGS84[1,2,3,4,5,6,7],\n"
               "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "        UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9122\"]],\n"
               "        AXIS[\"Latitude\",NORTH],\n"
               "        AXIS[\"Longitude\",EAST]],\n"
               "    PROJECTION[\"Transverse_Mercator\"],\n"
               "    PARAMETER[\"latitude_of_origin\",0],\n"
               "    PARAMETER[\"central_meridian\",3],\n"
               "    PARAMETER[\"scale_factor\",0.9996],\n"
               "    PARAMETER[\"false_easting\",500000],\n"
               "    PARAMETER[\"false_northing\",0],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]],\n"
               "    AXIS[\"Easting\",EAST],\n"
               "    AXIS[\"Northing\",NORTH]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->baseCRS()->nameStr(), "my PROJCRS");

    EXPECT_EQ(crs->hubCRS()->nameStr(), GeographicCRS::EPSG_4326->nameStr());

    ASSERT_TRUE(crs->transformation()->sourceCRS() != nullptr);
    EXPECT_EQ(crs->transformation()->sourceCRS()->nameStr(), "my GEOGCRS");

    auto params = crs->transformation()->getTOWGS84Parameters();
    auto expected = std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    ASSERT_EQ(params.size(), expected.size());
    for (int i = 0; i < 7; i++) {
        EXPECT_NEAR(params[i], expected[i], 1e-10);
    }
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, WKT1_VERT_DATUM_EXTENSION) {
    auto wkt = "VERT_CS[\"EGM2008 geoid height\",\n"
               "    VERT_DATUM[\"EGM2008 geoid\",2005,\n"
               "        EXTENSION[\"PROJ4_GRIDS\",\"egm08_25.gtx\"],\n"
               "        AUTHORITY[\"EPSG\",\"1027\"]],\n"
               "    UNIT[\"metre\",1,\n"
               "        AUTHORITY[\"EPSG\",\"9001\"]],\n"
               "    AXIS[\"Up\",UP],\n"
               "    AUTHORITY[\"EPSG\",\"3855\"]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->baseCRS()->nameStr(), "EGM2008 geoid height");

    EXPECT_EQ(crs->hubCRS()->nameStr(), GeographicCRS::EPSG_4979->nameStr());

    ASSERT_TRUE(crs->transformation()->sourceCRS() != nullptr);
    EXPECT_EQ(crs->transformation()->sourceCRS()->nameStr(),
              crs->baseCRS()->nameStr());

    ASSERT_TRUE(crs->transformation()->targetCRS() != nullptr);
    EXPECT_EQ(crs->transformation()->targetCRS()->nameStr(),
              crs->hubCRS()->nameStr());

    EXPECT_EQ(crs->transformation()->nameStr(),
              "EGM2008 geoid height to WGS84 ellipsoidal height");
    EXPECT_EQ(crs->transformation()->method()->nameStr(),
              "GravityRelatedHeight to Geographic3D");
    ASSERT_EQ(crs->transformation()->parameterValues().size(), 1);
    {
        const auto &opParamvalue =
            nn_dynamic_pointer_cast<OperationParameterValue>(
                crs->transformation()->parameterValues()[0]);
        ASSERT_TRUE(opParamvalue);
        const auto &paramName = opParamvalue->parameter()->nameStr();
        const auto &parameterValue = opParamvalue->parameterValue();
        EXPECT_TRUE(opParamvalue->parameter()->getEPSGCode() == 8666);
        EXPECT_EQ(paramName, "Geoid (height correction) model file");
        EXPECT_EQ(parameterValue->type(), ParameterValue::Type::FILENAME);
        EXPECT_EQ(parameterValue->valueFile(), "egm08_25.gtx");
    }
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, WKT1_DATUM_EXTENSION) {
    auto wkt =
        "PROJCS[\"unnamed\",\n"
        "    GEOGCS[\"International 1909 (Hayford)\",\n"
        "        DATUM[\"unknown\",\n"
        "            SPHEROID[\"intl\",6378388,297],\n"
        "            EXTENSION[\"PROJ4_GRIDS\",\"nzgd2kgrid0005.gsb\"]],\n"
        "        PRIMEM[\"Greenwich\",0],\n"
        "        UNIT[\"degree\",0.0174532925199433]],\n"
        "    PROJECTION[\"New_Zealand_Map_Grid\"],\n"
        "    PARAMETER[\"latitude_of_origin\",-41],\n"
        "    PARAMETER[\"central_meridian\",173],\n"
        "    PARAMETER[\"false_easting\",2510000],\n"
        "    PARAMETER[\"false_northing\",6023150],\n"
        "    UNIT[\"Meter\",1]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->baseCRS()->nameStr(), "unnamed");

    EXPECT_EQ(crs->hubCRS()->nameStr(), GeographicCRS::EPSG_4326->nameStr());

    ASSERT_TRUE(crs->transformation()->sourceCRS() != nullptr);
    EXPECT_EQ(crs->transformation()->sourceCRS()->nameStr(),
              crs->baseCRS()->nameStr());

    ASSERT_TRUE(crs->transformation()->targetCRS() != nullptr);
    EXPECT_EQ(crs->transformation()->targetCRS()->nameStr(),
              crs->hubCRS()->nameStr());

    EXPECT_EQ(crs->transformation()->nameStr(),
              "International 1909 (Hayford) to WGS84");
    EXPECT_EQ(crs->transformation()->method()->nameStr(), "NTv2");
    ASSERT_EQ(crs->transformation()->parameterValues().size(), 1);
    {
        const auto &opParamvalue =
            nn_dynamic_pointer_cast<OperationParameterValue>(
                crs->transformation()->parameterValues()[0]);
        ASSERT_TRUE(opParamvalue);
        const auto &paramName = opParamvalue->parameter()->nameStr();
        const auto &parameterValue = opParamvalue->parameterValue();
        EXPECT_TRUE(opParamvalue->parameter()->getEPSGCode() == 8656);
        EXPECT_EQ(paramName, "Latitude and longitude difference file");
        EXPECT_EQ(parameterValue->type(), ParameterValue::Type::FILENAME);
        EXPECT_EQ(parameterValue->valueFile(), "nzgd2kgrid0005.gsb");
    }
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, DerivedGeographicCRS_WKT2) {
    auto wkt = "GEODCRS[\"WMO Atlantic Pole\",\n"
               "    BASEGEODCRS[\"WGS 84\",\n"
               "        DATUM[\"World Geodetic System 1984\",\n"
               "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
               "                LENGTHUNIT[\"metre\",1]]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
               "    DERIVINGCONVERSION[\"Atlantic pole\",\n"
               "        METHOD[\"Pole rotation\"],\n"
               "        PARAMETER[\"Latitude of rotated pole\",52,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]]],\n"
               "        PARAMETER[\"Longitude of rotated pole\",-30,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]]],\n"
               "        PARAMETER[\"Axis rotation\",-25,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]]]],\n"
               "    CS[ellipsoidal,2],\n"
               "        AXIS[\"latitude\",north,\n"
               "            ORDER[1],\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]]],\n"
               "        AXIS[\"longitude\",east,\n"
               "            ORDER[2],\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<DerivedGeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "WMO Atlantic Pole");

    EXPECT_EQ(crs->baseCRS()->nameStr(), "WGS 84");
    EXPECT_TRUE(nn_dynamic_pointer_cast<GeographicCRS>(crs->baseCRS()) !=
                nullptr);

    EXPECT_EQ(crs->derivingConversion()->nameStr(), "Atlantic pole");

    EXPECT_TRUE(nn_dynamic_pointer_cast<EllipsoidalCS>(
                    crs->coordinateSystem()) != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, DerivedGeographicCRS_WKT2_2018) {
    auto wkt = "GEOGCRS[\"WMO Atlantic Pole\",\n"
               "    BASEGEOGCRS[\"WGS 84\",\n"
               "        DATUM[\"World Geodetic System 1984\",\n"
               "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
               "                LENGTHUNIT[\"metre\",1]]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
               "    DERIVINGCONVERSION[\"Atlantic pole\",\n"
               "        METHOD[\"Pole rotation\"],\n"
               "        PARAMETER[\"Latitude of rotated pole\",52,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]]],\n"
               "        PARAMETER[\"Longitude of rotated pole\",-30,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]]],\n"
               "        PARAMETER[\"Axis rotation\",-25,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]]]],\n"
               "    CS[ellipsoidal,2],\n"
               "        AXIS[\"latitude\",north,\n"
               "            ORDER[1],\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]]],\n"
               "        AXIS[\"longitude\",east,\n"
               "            ORDER[2],\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
               "                ID[\"EPSG\",9122]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<DerivedGeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "WMO Atlantic Pole");

    EXPECT_EQ(crs->baseCRS()->nameStr(), "WGS 84");
    EXPECT_TRUE(nn_dynamic_pointer_cast<GeographicCRS>(crs->baseCRS()) !=
                nullptr);

    EXPECT_EQ(crs->derivingConversion()->nameStr(), "Atlantic pole");

    EXPECT_TRUE(nn_dynamic_pointer_cast<EllipsoidalCS>(
                    crs->coordinateSystem()) != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, DerivedGeodeticCRS) {
    auto wkt = "GEODCRS[\"Derived geodetic CRS\",\n"
               "    BASEGEODCRS[\"WGS 84\",\n"
               "        DATUM[\"World Geodetic System 1984\",\n"
               "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
               "                LENGTHUNIT[\"metre\",1]]],\n"
               "        PRIMEM[\"Greenwich\",0,\n"
               "            ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
               "    DERIVINGCONVERSION[\"Some conversion\",\n"
               "        METHOD[\"Some method\"],\n"
               "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
               "    CS[Cartesian,3],\n"
               "        AXIS[\"(X)\",geocentricX,\n"
               "            ORDER[1],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]],\n"
               "        AXIS[\"(Y)\",geocentricY,\n"
               "            ORDER[2],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]],\n"
               "        AXIS[\"(Z)\",geocentricZ,\n"
               "            ORDER[3],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]]]";
    ;

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<DerivedGeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "Derived geodetic CRS");

    EXPECT_EQ(crs->baseCRS()->nameStr(), "WGS 84");
    EXPECT_TRUE(nn_dynamic_pointer_cast<GeographicCRS>(crs->baseCRS()) !=
                nullptr);

    EXPECT_EQ(crs->derivingConversion()->nameStr(), "Some conversion");

    EXPECT_TRUE(nn_dynamic_pointer_cast<CartesianCS>(crs->coordinateSystem()) !=
                nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, DerivedProjectedCRS) {
    auto wkt =
        "DERIVEDPROJCRS[\"derived projectedCRS\",\n"
        "    BASEPROJCRS[\"WGS 84 / UTM zone 31N\",\n"
        "        BASEGEOGCRS[\"WGS 84\",\n"
        "            DATUM[\"World Geodetic System 1984\",\n"
        "                ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                    LENGTHUNIT[\"metre\",1]]],\n"
        "            PRIMEM[\"Greenwich\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "        CONVERSION[\"UTM zone 31N\",\n"
        "            METHOD[\"Transverse Mercator\",\n"
        "                ID[\"EPSG\",9807]],\n"
        "            PARAMETER[\"Latitude of natural origin\",0,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8801]],\n"
        "            PARAMETER[\"Longitude of natural origin\",3,\n"
        "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "                ID[\"EPSG\",8802]],\n"
        "            PARAMETER[\"Scale factor at natural origin\",0.9996,\n"
        "                SCALEUNIT[\"unity\",1],\n"
        "                ID[\"EPSG\",8805]],\n"
        "            PARAMETER[\"False easting\",500000,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8806]],\n"
        "            PARAMETER[\"False northing\",0,\n"
        "                LENGTHUNIT[\"metre\",1],\n"
        "                ID[\"EPSG\",8807]]]],\n"
        "    DERIVINGCONVERSION[\"unnamed\",\n"
        "        METHOD[\"PROJ unimplemented\"],\n"
        "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
        "    CS[Cartesian,2],\n"
        "        AXIS[\"(E)\",east,\n"
        "            ORDER[1],\n"
        "            LENGTHUNIT[\"metre\",1,\n"
        "                ID[\"EPSG\",9001]]],\n"
        "        AXIS[\"(N)\",north,\n"
        "            ORDER[2],\n"
        "            LENGTHUNIT[\"metre\",1,\n"
        "                ID[\"EPSG\",9001]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<DerivedProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "derived projectedCRS");

    EXPECT_EQ(crs->baseCRS()->nameStr(), "WGS 84 / UTM zone 31N");
    EXPECT_TRUE(nn_dynamic_pointer_cast<ProjectedCRS>(crs->baseCRS()) !=
                nullptr);

    EXPECT_EQ(crs->derivingConversion()->nameStr(), "unnamed");

    EXPECT_TRUE(nn_dynamic_pointer_cast<CartesianCS>(crs->coordinateSystem()) !=
                nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, DerivedProjectedCRS_ordinal) {
    auto wkt = "DERIVEDPROJCRS[\"derived projectedCRS\",\n"
               "    BASEPROJCRS[\"BASEPROJCRS\",\n"
               "        BASEGEOGCRS[\"WGS 84\",\n"
               "            DATUM[\"World Geodetic System 1984\",\n"
               "                ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
               "                    LENGTHUNIT[\"metre\",1]]],\n"
               "            PRIMEM[\"Greenwich\",0,\n"
               "                ANGLEUNIT[\"degree\",0.0174532925199433],\n"
               "                ID[\"EPSG\",8901]]],\n"
               "        CONVERSION[\"unnamed\",\n"
               "            METHOD[\"PROJ unimplemented\"],\n"
               "            PARAMETER[\"foo\",1,\n"
               "                LENGTHUNIT[\"metre\",1,\n"
               "                    ID[\"EPSG\",9001]]]]],\n"
               "    DERIVINGCONVERSION[\"unnamed\",\n"
               "        METHOD[\"PROJ unimplemented\"],\n"
               "        PARAMETER[\"foo\",1,\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]]],\n"
               "    CS[ordinal,2],\n"
               "        AXIS[\"inline (I)\",northNorthWest,\n"
               "            ORDER[1]],\n"
               "        AXIS[\"crossline (J)\",westSouthWest,\n"
               "            ORDER[2]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<DerivedProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_TRUE(nn_dynamic_pointer_cast<OrdinalCS>(crs->coordinateSystem()) !=
                nullptr);

    EXPECT_EQ(
        crs->exportToWKT(
            WKTFormatter::create(WKTFormatter::Convention::WKT2_2018).get()),
        wkt);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, TemporalDatum) {
    auto wkt = "TDATUM[\"Gregorian calendar\",\n"
               "    CALENDAR[\"my calendar\"],\n"
               "    TIMEORIGIN[0000-01-01]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto tdatum = nn_dynamic_pointer_cast<TemporalDatum>(obj);
    ASSERT_TRUE(tdatum != nullptr);

    EXPECT_EQ(tdatum->nameStr(), "Gregorian calendar");
    EXPECT_EQ(tdatum->temporalOrigin().toString(), "0000-01-01");
    EXPECT_EQ(tdatum->calendar(), "my calendar");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, TemporalDatum_no_calendar) {
    auto wkt = "TDATUM[\"Gregorian calendar\",\n"
               "    TIMEORIGIN[0000-01-01]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto tdatum = nn_dynamic_pointer_cast<TemporalDatum>(obj);
    ASSERT_TRUE(tdatum != nullptr);

    EXPECT_EQ(tdatum->nameStr(), "Gregorian calendar");
    EXPECT_EQ(tdatum->temporalOrigin().toString(), "0000-01-01");
    EXPECT_EQ(tdatum->calendar(), "proleptic Gregorian");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, dateTimeTemporalCRS_WKT2) {
    auto wkt = "TIMECRS[\"Temporal CRS\",\n"
               "    TDATUM[\"Gregorian calendar\",\n"
               "        TIMEORIGIN[0000-01-01]],\n"
               "    CS[temporal,1],\n"
               "        AXIS[\"time (T)\",future]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<TemporalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "Temporal CRS");
    auto tdatum = crs->datum();
    EXPECT_EQ(tdatum->nameStr(), "Gregorian calendar");
    EXPECT_EQ(tdatum->temporalOrigin().toString(), "0000-01-01");
    EXPECT_EQ(tdatum->calendar(), "proleptic Gregorian");
    EXPECT_TRUE(nn_dynamic_pointer_cast<DateTimeTemporalCS>(
                    crs->coordinateSystem()) != nullptr);
    ASSERT_EQ(crs->coordinateSystem()->axisList().size(), 1);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->unit().type(),
              UnitOfMeasure::Type::NONE);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->unit().name(), "");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, dateTimeTemporalCRS_WKT2_2018) {
    auto wkt = "TIMECRS[\"Temporal CRS\",\n"
               "    TDATUM[\"Gregorian calendar\",\n"
               "        CALENDAR[\"proleptic Gregorian\"],\n"
               "        TIMEORIGIN[0000-01-01]],\n"
               "    CS[TemporalDateTime,1],\n"
               "        AXIS[\"time (T)\",future]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<TemporalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "Temporal CRS");
    auto tdatum = crs->datum();
    EXPECT_EQ(tdatum->nameStr(), "Gregorian calendar");
    EXPECT_EQ(tdatum->temporalOrigin().toString(), "0000-01-01");
    EXPECT_EQ(tdatum->calendar(), "proleptic Gregorian");
    EXPECT_TRUE(nn_dynamic_pointer_cast<DateTimeTemporalCS>(
                    crs->coordinateSystem()) != nullptr);
    ASSERT_EQ(crs->coordinateSystem()->axisList().size(), 1);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->unit().type(),
              UnitOfMeasure::Type::NONE);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->unit().name(), "");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, temporalCountCRSWithConvFactor_WKT2_2018) {
    auto wkt = "TIMECRS[\"GPS milliseconds\",\n"
               "    TDATUM[\"GPS time origin\",\n"
               "        TIMEORIGIN[1980-01-01T00:00:00.0Z]],\n"
               "    CS[TemporalCount,1],\n"
               "        AXIS[\"(T)\",future,\n"
               "            TIMEUNIT[\"milliseconds (ms)\",0.001]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<TemporalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "GPS milliseconds");
    auto tdatum = crs->datum();
    EXPECT_EQ(tdatum->nameStr(), "GPS time origin");
    EXPECT_EQ(tdatum->temporalOrigin().toString(), "1980-01-01T00:00:00.0Z");
    EXPECT_EQ(tdatum->calendar(), "proleptic Gregorian");
    EXPECT_TRUE(nn_dynamic_pointer_cast<TemporalCountCS>(
                    crs->coordinateSystem()) != nullptr);
    ASSERT_EQ(crs->coordinateSystem()->axisList().size(), 1);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->unit().name(),
              "milliseconds (ms)");
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->unit().conversionToSI(),
              0.001);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, temporalCountCRSWithoutConvFactor_WKT2_2018) {
    auto wkt = "TIMECRS[\"Calendar hours from 1979-12-29\",\n"
               "    TDATUM[\"29 December 1979\",\n"
               "        CALENDAR[\"proleptic Gregorian\"],\n"
               "        TIMEORIGIN[1979-12-29T00Z]],\n"
               "    CS[TemporalCount,1],\n"
               "        AXIS[\"time\",future,\n"
               "            TIMEUNIT[\"hour\"]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<TemporalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "Calendar hours from 1979-12-29");
    auto tdatum = crs->datum();
    EXPECT_EQ(tdatum->nameStr(), "29 December 1979");
    EXPECT_EQ(tdatum->temporalOrigin().toString(), "1979-12-29T00Z");
    EXPECT_TRUE(nn_dynamic_pointer_cast<TemporalCountCS>(
                    crs->coordinateSystem()) != nullptr);
    ASSERT_EQ(crs->coordinateSystem()->axisList().size(), 1);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->unit().name(), "hour");
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->unit().conversionToSI(),
              0.0);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, temporalMeasureCRSWithoutConvFactor_WKT2_2018) {
    auto wkt = "TIMECRS[\"Decimal Years CE\",\n"
               "    TIMEDATUM[\"Common Era\",\n"
               "        TIMEORIGIN[0000]],\n"
               "    CS[TemporalMeasure,1],\n"
               "        AXIS[\"decimal years (a)\",future,\n"
               "            TIMEUNIT[\"year\"]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<TemporalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "Decimal Years CE");
    auto tdatum = crs->datum();
    EXPECT_EQ(tdatum->nameStr(), "Common Era");
    EXPECT_EQ(tdatum->temporalOrigin().toString(), "0000");
    EXPECT_TRUE(nn_dynamic_pointer_cast<TemporalMeasureCS>(
                    crs->coordinateSystem()) != nullptr);
    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 1);
    auto axis = cs->axisList()[0];
    EXPECT_EQ(axis->nameStr(), "Decimal years");
    EXPECT_EQ(axis->unit().name(), "year");
    EXPECT_EQ(axis->unit().conversionToSI(), 0.0);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, EDATUM) {
    auto wkt = "EDATUM[\"Engineering datum\",\n"
               "    ANCHOR[\"my anchor\"]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto edatum = nn_dynamic_pointer_cast<EngineeringDatum>(obj);
    ASSERT_TRUE(edatum != nullptr);

    EXPECT_EQ(edatum->nameStr(), "Engineering datum");
    auto anchor = edatum->anchorDefinition();
    EXPECT_TRUE(anchor.has_value());
    EXPECT_EQ(*anchor, "my anchor");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, ENGINEERINGDATUM) {
    auto wkt = "ENGINEERINGDATUM[\"Engineering datum\"]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto edatum = nn_dynamic_pointer_cast<EngineeringDatum>(obj);
    ASSERT_TRUE(edatum != nullptr);

    EXPECT_EQ(edatum->nameStr(), "Engineering datum");
    auto anchor = edatum->anchorDefinition();
    EXPECT_TRUE(!anchor.has_value());
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, ENGCRS) {
    auto wkt = "ENGCRS[\"Engineering CRS\",\n"
               "    EDATUM[\"Engineering datum\"],\n"
               "    CS[Cartesian,2],\n"
               "        AXIS[\"(E)\",east,\n"
               "            ORDER[1],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]],\n"
               "        AXIS[\"(N)\",north,\n"
               "            ORDER[2],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<EngineeringCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "Engineering CRS");
    EXPECT_EQ(crs->datum()->nameStr(), "Engineering datum");
    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 2);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, ENGINEERINGCRS) {
    auto wkt = "ENGINEERINGCRS[\"Engineering CRS\",\n"
               "    ENGINEERINGDATUM[\"Engineering datum\"],\n"
               "    CS[Cartesian,2],\n"
               "        AXIS[\"(E)\",east,\n"
               "            ORDER[1],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]],\n"
               "        AXIS[\"(N)\",north,\n"
               "            ORDER[2],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<EngineeringCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "Engineering CRS");
    EXPECT_EQ(crs->datum()->nameStr(), "Engineering datum");
    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 2);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, LOCAL_CS_short) {
    auto wkt = "LOCAL_CS[\"Engineering CRS\"]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<EngineeringCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "Engineering CRS");
    EXPECT_FALSE(!crs->datum()->nameStr().empty());
    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 2);

    EXPECT_EQ(
        crs->exportToWKT(
            WKTFormatter::create(WKTFormatter::Convention::WKT1_GDAL).get()),
        wkt);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, LOCAL_CS_long_one_axis) {
    auto wkt = "LOCAL_CS[\"Engineering CRS\",\n"
               "    LOCAL_DATUM[\"Engineering datum\",12345],\n"
               "    UNIT[\"meter\",1],\n"
               "    AXIS[\"height\",up]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<EngineeringCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "Engineering CRS");
    EXPECT_EQ(crs->datum()->nameStr(), "Engineering datum");
    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 1);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, LOCAL_CS_long_two_axis) {
    auto wkt = "LOCAL_CS[\"Engineering CRS\",\n"
               "    LOCAL_DATUM[\"Engineering datum\",12345],\n"
               "    UNIT[\"meter\",1],\n"
               "    AXIS[\"Easting\",EAST],\n"
               "    AXIS[\"Northing\",NORTH]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<EngineeringCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "Engineering CRS");
    EXPECT_EQ(crs->datum()->nameStr(), "Engineering datum");
    auto cs = crs->coordinateSystem();
    ASSERT_EQ(cs->axisList().size(), 2);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, PDATUM) {
    auto wkt = "PDATUM[\"Parametric datum\",\n"
               "    ANCHOR[\"my anchor\"]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto datum = nn_dynamic_pointer_cast<ParametricDatum>(obj);
    ASSERT_TRUE(datum != nullptr);

    EXPECT_EQ(datum->nameStr(), "Parametric datum");
    auto anchor = datum->anchorDefinition();
    EXPECT_TRUE(anchor.has_value());
    EXPECT_EQ(*anchor, "my anchor");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, PARAMETRICDATUM) {
    auto wkt = "PARAMETRICDATUM[\"Parametric datum\",\n"
               "    ANCHOR[\"my anchor\"]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto datum = nn_dynamic_pointer_cast<ParametricDatum>(obj);
    ASSERT_TRUE(datum != nullptr);

    EXPECT_EQ(datum->nameStr(), "Parametric datum");
    auto anchor = datum->anchorDefinition();
    EXPECT_TRUE(anchor.has_value());
    EXPECT_EQ(*anchor, "my anchor");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, PARAMETRICCRS) {
    auto wkt =
        "PARAMETRICCRS[\"WMO standard atmosphere layer 0\","
        "     PDATUM[\"Mean Sea Level\",ANCHOR[\"1013.25 hPa at 15°C\"]],"
        "     CS[parametric,1],"
        "         AXIS[\"pressure (hPa)\",up],"
        "         PARAMETRICUNIT[\"HectoPascal\",100.0]"
        "     ]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ParametricCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "WMO standard atmosphere layer 0");
    EXPECT_EQ(crs->datum()->nameStr(), "Mean Sea Level");
    auto cs = crs->coordinateSystem();
    EXPECT_TRUE(nn_dynamic_pointer_cast<ParametricCS>(cs) != nullptr);
    ASSERT_EQ(cs->axisList().size(), 1);
    auto axis = cs->axisList()[0];
    EXPECT_EQ(axis->nameStr(), "Pressure");
    EXPECT_EQ(axis->unit().name(), "HectoPascal");
    EXPECT_EQ(axis->unit().type(), UnitOfMeasure::Type::PARAMETRIC);
    EXPECT_EQ(axis->unit().conversionToSI(), 100.0);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, PARAMETRICCRS_PARAMETRICDATUM) {
    auto wkt = "PARAMETRICCRS[\"WMO standard atmosphere layer 0\","
               "     PARAMETRICDATUM[\"Mean Sea Level\"],"
               "     CS[parametric,1],"
               "         AXIS[\"pressure (hPa)\",up],"
               "         PARAMETRICUNIT[\"HectoPascal\",100.0]"
               "     ]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ParametricCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, DerivedVerticalCRS) {
    auto wkt = "VERTCRS[\"Derived vertCRS\",\n"
               "    BASEVERTCRS[\"ODN height\",\n"
               "        VDATUM[\"Ordnance Datum Newlyn\"]],\n"
               "    DERIVINGCONVERSION[\"unnamed\",\n"
               "        METHOD[\"PROJ unimplemented\"],\n"
               "        PARAMETER[\"foo\",1,\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]]],\n"
               "    CS[vertical,1],\n"
               "        AXIS[\"gravity-related height (H)\",up,\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<DerivedVerticalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, DerivedEngineeringCRS) {

    auto wkt = "ENGCRS[\"Derived EngineeringCRS\",\n"
               "    BASEENGCRS[\"Engineering CRS\",\n"
               "        EDATUM[\"Engineering datum\"]],\n"
               "    DERIVINGCONVERSION[\"unnamed\",\n"
               "        METHOD[\"PROJ unimplemented\"],\n"
               "        PARAMETER[\"foo\",1,\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]]],\n"
               "    CS[Cartesian,2],\n"
               "        AXIS[\"(E)\",east,\n"
               "            ORDER[1],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]],\n"
               "        AXIS[\"(N)\",north,\n"
               "            ORDER[2],\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<DerivedEngineeringCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, DerivedParametricCRS) {

    auto wkt = "PARAMETRICCRS[\"Derived ParametricCRS\",\n"
               "    BASEPARAMCRS[\"Parametric CRS\",\n"
               "        PDATUM[\"Parametric datum\"]],\n"
               "    DERIVINGCONVERSION[\"unnamed\",\n"
               "        METHOD[\"PROJ unimplemented\"],\n"
               "        PARAMETER[\"foo\",1,\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]]],\n"
               "    CS[parametric,1],\n"
               "        AXIS[\"pressure (hPa)\",up,\n"
               "            PARAMETRICUNIT[\"HectoPascal\",100]]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<DerivedParametricCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, DerivedTemporalCRS) {

    auto wkt = "TIMECRS[\"Derived TemporalCRS\",\n"
               "    BASETIMECRS[\"Temporal CRS\",\n"
               "        TDATUM[\"Gregorian calendar\",\n"
               "            CALENDAR[\"proleptic Gregorian\"],\n"
               "            TIMEORIGIN[0000-01-01]]],\n"
               "    DERIVINGCONVERSION[\"unnamed\",\n"
               "        METHOD[\"PROJ unimplemented\"],\n"
               "        PARAMETER[\"foo\",1,\n"
               "            LENGTHUNIT[\"metre\",1,\n"
               "                ID[\"EPSG\",9001]]]],\n"
               "    CS[TemporalDateTime,1],\n"
               "        AXIS[\"time (T)\",future]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<DerivedTemporalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, ensemble) {
    auto wkt = "ENSEMBLE[\"test\",\n"
               "    MEMBER[\"World Geodetic System 1984\",\n"
               "        ID[\"EPSG\",6326]],\n"
               "    MEMBER[\"other datum\"],\n"
               "    ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
               "        LENGTHUNIT[\"metre\",1],\n"
               "        ID[\"EPSG\",7030]],\n"
               "    ENSEMBLEACCURACY[100]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto ensemble = nn_dynamic_pointer_cast<DatumEnsemble>(obj);
    ASSERT_TRUE(ensemble != nullptr);

    ASSERT_EQ(ensemble->datums().size(), 2);
    auto firstDatum =
        nn_dynamic_pointer_cast<GeodeticReferenceFrame>(ensemble->datums()[0]);
    ASSERT_TRUE(firstDatum != nullptr);
    EXPECT_EQ(firstDatum->nameStr(), "World Geodetic System 1984");
    ASSERT_EQ(firstDatum->identifiers().size(), 1);
    EXPECT_EQ(firstDatum->identifiers()[0]->code(), "6326");
    EXPECT_EQ(*(firstDatum->identifiers()[0]->codeSpace()), "EPSG");

    EXPECT_EQ(firstDatum->ellipsoid()->nameStr(), "WGS 84");

    EXPECT_EQ(ensemble->positionalAccuracy()->value(), "100");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, ensemble_vdatum) {
    auto wkt = "ENSEMBLE[\"unnamed\",\n"
               "    MEMBER[\"vdatum1\"],\n"
               "    MEMBER[\"vdatum2\"],\n"
               "    ENSEMBLEACCURACY[100]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto ensemble = nn_dynamic_pointer_cast<DatumEnsemble>(obj);
    ASSERT_TRUE(ensemble != nullptr);

    ASSERT_EQ(ensemble->datums().size(), 2);
    auto firstDatum =
        nn_dynamic_pointer_cast<VerticalReferenceFrame>(ensemble->datums()[0]);
    ASSERT_TRUE(firstDatum != nullptr);
    EXPECT_EQ(firstDatum->nameStr(), "vdatum1");

    EXPECT_EQ(ensemble->positionalAccuracy()->value(), "100");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, esri_geogcs_datum_spheroid_name_hardcoded_substitution) {
    auto wkt = "GEOGCS[\"GCS_WGS_1984\",DATUM[\"D_WGS_1984\","
               "SPHEROID[\"WGS_1984\",6378137.0,298.257223563]],"
               "PRIMEM[\"Greenwich\",0.0],"
               "UNIT[\"Degree\",0.0174532925199433]]";

    // Test substitutions of CRS, datum and ellipsoid names from ESRI names
    // to EPSG names.
    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "WGS 84");
    EXPECT_EQ(crs->datum()->nameStr(), "World Geodetic System 1984");
    EXPECT_EQ(crs->ellipsoid()->nameStr(), "WGS 84");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, esri_geogcs_datum_spheroid_name_from_db_substitution) {
    auto wkt = "GEOGCS[\"GCS_WGS_1966\",DATUM[\"D_WGS_1966\","
               "SPHEROID[\"WGS_1966\",6378145.0,298.25]],"
               "PRIMEM[\"Greenwich\",0.0],"
               "UNIT[\"Degree\",0.0174532925199433]]";

    // Test substitutions of CRS, datum and ellipsoid names from ESRI names
    // to EPSG names.
    auto obj = WKTParser()
                   .attachDatabaseContext(DatabaseContext::create())
                   .createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "WGS 66");
    EXPECT_EQ(crs->datum()->nameStr(), "World Geodetic System 1966");
    EXPECT_EQ(crs->ellipsoid()->nameStr(), "WGS_1966");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, esri_datum_name_with_prime_meridian) {
    auto wkt = "GEOGCS[\"GCS_NTF_Paris\",DATUM[\"D_NTF\","
               "SPHEROID[\"Clarke_1880_IGN\",6378249.2,293.4660212936265]],"
               "PRIMEM[\"Paris\",2.337229166666667],"
               "UNIT[\"Grad\",0.01570796326794897]]";

    // D_NTF normally translates to "Nouvelle Triangulation Francaise",
    // but as we have a non-Greenwich prime meridian, we also test if
    // "Nouvelle Triangulation Francaise (Paris)" is not a registered datum name
    auto obj = WKTParser()
                   .attachDatabaseContext(DatabaseContext::create())
                   .createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "NTF (Paris)");
    EXPECT_EQ(crs->datum()->nameStr(),
              "Nouvelle Triangulation Francaise (Paris)");
    EXPECT_EQ(crs->ellipsoid()->nameStr(), "Clarke 1880 (IGN)");
}

// ---------------------------------------------------------------------------

static const struct {
    const char *esriProjectionName;
    std::vector<std::pair<const char *, double>> esriParams;
    const char *wkt2ProjectionName;
    std::vector<std::pair<const char *, double>> wkt2Params;
} esriProjDefs[] = {

    {"Plate_Carree",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Equidistant Cylindrical",
     {
         {"Latitude of 1st standard parallel", 0},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Equidistant_Cylindrical",
     {
         {"False_Easting", 1},
         {"False_Northing", 2},
         {"Central_Meridian", 3},
         {"Standard_Parallel_1", 4},
     },
     "Equidistant Cylindrical",
     {
         {"Latitude of 1st standard parallel", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Miller_Cylindrical",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Miller Cylindrical",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Mercator",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4}},
     "Mercator (variant B)",
     {
         {"Latitude of 1st standard parallel", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Gauss_Kruger",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Scale_Factor", 4},
      {"Latitude_Of_Origin", 5}},
     "Transverse Mercator",
     {
         {"Latitude of natural origin", 5},
         {"Longitude of natural origin", 3},
         {"Scale factor at natural origin", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Transverse_Mercator",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Scale_Factor", 4},
      {"Latitude_Of_Origin", 5}},
     "Transverse Mercator",
     {
         {"Latitude of natural origin", 5},
         {"Longitude of natural origin", 3},
         {"Scale factor at natural origin", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Albers",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4},
      {"Standard_Parallel_2", 5},
      {"Latitude_Of_Origin", 6}},
     "Albers Equal Area",
     {
         {"Latitude of false origin", 6},
         {"Longitude of false origin", 3},
         {"Latitude of 1st standard parallel", 4},
         {"Latitude of 2nd standard parallel", 5},
         {"Easting at false origin", 1},
         {"Northing at false origin", 2},
     }},

    {"Sinusoidal",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Sinusoidal",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Mollweide",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Mollweide",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Eckert_I",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Eckert I",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    // skipping Eckert_II to Eckert_VI

    {"Gall_Stereographic",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Gall Stereographic",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Winkel_I",
     {
         {"False_Easting", 1},
         {"False_Northing", 2},
         {"Central_Meridian", 3},
         {"Standard_Parallel_1", 4},
     },
     "Winkel I",
     {
         {"Longitude of natural origin", 3},
         {"Latitude of 1st standard parallel", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Winkel_II",
     {
         {"False_Easting", 1},
         {"False_Northing", 2},
         {"Central_Meridian", 3},
         {"Standard_Parallel_1", 4},
     },
     "Winkel II",
     {
         {"Longitude of natural origin", 3},
         {"Latitude of 1st standard parallel", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Lambert_Conformal_Conic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4},
      {"Scale_Factor", 5},
      {"Latitude_Of_Origin", 4}},
     "Lambert Conic Conformal (1SP)",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"Scale factor at natural origin", 5},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Lambert_Conformal_Conic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4},
      {"Standard_Parallel_2", 5},
      {"Latitude_Of_Origin", 6}},
     "Lambert Conic Conformal (2SP)",
     {
         {"Latitude of false origin", 6},
         {"Longitude of false origin", 3},
         {"Latitude of 1st standard parallel", 4},
         {"Latitude of 2nd standard parallel", 5},
         {"Easting at false origin", 1},
         {"Northing at false origin", 2},
     }},

    // Unusual variant of above with Scale_Factor=1
    {"Lambert_Conformal_Conic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4},
      {"Standard_Parallel_2", 5},
      {"Scale_Factor", 1.0},
      {"Latitude_Of_Origin", 6}},
     "Lambert Conic Conformal (2SP)",
     {
         {"Latitude of false origin", 6},
         {"Longitude of false origin", 3},
         {"Latitude of 1st standard parallel", 4},
         {"Latitude of 2nd standard parallel", 5},
         {"Easting at false origin", 1},
         {"Northing at false origin", 2},
     }},

    {"Polyconic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Latitude_Of_Origin", 4}},
     "American Polyconic",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Quartic_Authalic",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Quartic Authalic",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Loximuthal",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Central_Parallel", 4}},
     "Loximuthal",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Bonne",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4}},
     "Bonne",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Hotine_Oblique_Mercator_Two_Point_Natural_Origin",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Latitude_Of_1st_Point", 3},
      {"Latitude_Of_2nd_Point", 4},
      {"Scale_Factor", 5},
      {"Longitude_Of_1st_Point", 6},
      {"Longitude_Of_2nd_Point", 7},
      {"Latitude_Of_Center", 8}},
     "Hotine Oblique Mercator Two Point Natural Origin",
     {
         {"Latitude of projection centre", 8},
         {"Latitude of 1st point", 3},
         {"Longitude of 1st point", 6},
         {"Latitude of 2nd point", 4},
         {"Longitude of 2nd point", 7},
         {"Scale factor on initial line", 5},
         {"Easting at projection centre", 1},
         {"Northing at projection centre", 2},
     }},

    {"Stereographic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Scale_Factor", 4},
      {"Latitude_Of_Origin", 5}},
     "Stereographic",
     {
         {"Latitude of natural origin", 5},
         {"Longitude of natural origin", 3},
         {"Scale factor at natural origin", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Stereographic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Scale_Factor", 4},
      {"Latitude_Of_Origin", 90}},
     "Polar Stereographic (variant A)",
     {
         {"Latitude of natural origin", 90},
         {"Longitude of natural origin", 3},
         {"Scale factor at natural origin", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Stereographic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Scale_Factor", 4},
      {"Latitude_Of_Origin", -90}},
     "Polar Stereographic (variant A)",
     {
         {"Latitude of natural origin", -90},
         {"Longitude of natural origin", 3},
         {"Scale factor at natural origin", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Equidistant_Conic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4},
      {"Standard_Parallel_2", 5},
      {"Latitude_Of_Origin", 6}},
     "Equidistant Conic",
     {
         {"Latitude of natural origin", 6},
         {"Longitude of natural origin", 3},
         {"Latitude of 1st standard parallel", 4},
         {"Latitude of 2nd standard parallel", 5},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Cassini",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Scale_Factor", 1},
      {"Latitude_Of_Origin", 4}},
     "Cassini-Soldner",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Van_der_Grinten_I",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Van Der Grinten",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Robinson",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Robinson",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Two_Point_Equidistant",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Latitude_Of_1st_Point", 3},
      {"Latitude_Of_2nd_Point", 4},
      {"Longitude_Of_1st_Point", 5},
      {"Longitude_Of_2nd_Point", 6}},
     "Two Point Equidistant",
     {
         {"Latitude of 1st point", 3},
         {"Longitude of 1st point", 5},
         {"Latitude of 2nd point", 4},
         {"Longitude of 2nd point", 6},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Azimuthal_Equidistant",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Latitude_Of_Origin", 4}},
     "Modified Azimuthal Equidistant",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Lambert_Azimuthal_Equal_Area",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Latitude_Of_Origin", 4}},
     "Lambert Azimuthal Equal Area",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Cylindrical_Equal_Area",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4}},
     "Lambert Cylindrical Equal Area (Spherical)",
     {
         {"Latitude of 1st standard parallel", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    // Untested: Hotine_Oblique_Mercator_Two_Point_Center

    {"Hotine_Oblique_Mercator_Azimuth_Natural_Origin",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Scale_Factor", 3},
      {"Azimuth", 4},
      {"Longitude_Of_Center", 5},
      {"Latitude_Of_Center", 6}},
     "Hotine Oblique Mercator (variant A)",
     {
         {"Latitude of projection centre", 6},
         {"Longitude of projection centre", 5},
         {"Azimuth of initial line", 4},
         {"Angle from Rectified to Skew Grid", 4},
         {"Scale factor on initial line", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Hotine_Oblique_Mercator_Azimuth_Center",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Scale_Factor", 3},
      {"Azimuth", 4},
      {"Longitude_Of_Center", 5},
      {"Latitude_Of_Center", 6}},
     "Hotine Oblique Mercator (variant B)",
     {
         {"Latitude of projection centre", 6},
         {"Longitude of projection centre", 5},
         {"Azimuth of initial line", 4},
         {"Angle from Rectified to Skew Grid", 4},
         {"Scale factor on initial line", 3},
         {"Easting at projection centre", 1},
         {"Northing at projection centre", 2},
     }},

    {"Double_Stereographic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Scale_Factor", 4},
      {"Latitude_Of_Origin", 5}},
     "Oblique Stereographic",
     {
         {"Latitude of natural origin", 5},
         {"Longitude of natural origin", 3},
         {"Scale factor at natural origin", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Krovak",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Pseudo_Standard_Parallel_1", 3},
      {"Scale_Factor", 4},
      {"Azimuth", 5},
      {"Longitude_Of_Center", 6},
      {"Latitude_Of_Center", 7},
      {"X_Scale", 1},
      {"Y_Scale", 1},
      {"XY_Plane_Rotation", 0}},
     "Krovak",
     {
         {"Latitude of projection centre", 7},
         {"Longitude of origin", 6},
         {"Co-latitude of cone axis", 5},
         {"Latitude of pseudo standard parallel", 3},
         {"Scale factor on pseudo standard parallel", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Krovak",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Pseudo_Standard_Parallel_1", 3},
      {"Scale_Factor", 4},
      {"Azimuth", 5},
      {"Longitude_Of_Center", 6},
      {"Latitude_Of_Center", 7},
      {"X_Scale", -1},
      {"Y_Scale", 1},
      {"XY_Plane_Rotation", 90}},
     "Krovak (North Orientated)",
     {
         {"Latitude of projection centre", 7},
         {"Longitude of origin", 6},
         {"Co-latitude of cone axis", 5},
         {"Latitude of pseudo standard parallel", 3},
         {"Scale factor on pseudo standard parallel", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"New_Zealand_Map_Grid",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Longitude_Of_Origin", 3},
      {"Latitude_Of_Origin", 4}},
     "New Zealand Map Grid",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Orthographic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Longitude_Of_Center", 3},
      {"Latitude_Of_Center", 4}},
     "Orthographic",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Winkel_Tripel",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4}},
     "Winkel Tripel",
     {
         {"Longitude of natural origin", 3},
         {"Latitude of 1st standard parallel", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Aitoff",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Aitoff",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Craster_Parabolic",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Craster Parabolic",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Gnomonic",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Longitude_Of_Center", 3},
      {"Latitude_Of_Center", 4}},
     "Gnomonic",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Stereographic_North_Pole",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4}},
     "Polar Stereographic (variant B)",
     {
         {"Latitude of standard parallel", 4},
         {"Longitude of origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Stereographic_South_Pole",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", -4}},
     "Polar Stereographic (variant B)",
     {
         {"Latitude of standard parallel", -4},
         {"Longitude of origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Rectified_Skew_Orthomorphic_Natural_Origin",
     {
         {"False_Easting", 1},
         {"False_Northing", 2},
         {"Scale_Factor", 3},
         {"Azimuth", 4},
         {"Longitude_Of_Center", 5},
         {"Latitude_Of_Center", 6},
         {"XY_Plane_Rotation", 7},
     },
     "Hotine Oblique Mercator (variant A)",
     {
         {"Latitude of projection centre", 6},
         {"Longitude of projection centre", 5},
         {"Azimuth of initial line", 4},
         {"Angle from Rectified to Skew Grid", 7},
         {"Scale factor on initial line", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    // Temptative mapping
    {"Rectified_Skew_Orthomorphic_Center",
     {
         {"False_Easting", 1},
         {"False_Northing", 2},
         {"Scale_Factor", 3},
         {"Azimuth", 4},
         {"Longitude_Of_Center", 5},
         {"Latitude_Of_Center", 6},
         {"XY_Plane_Rotation", 7},
     },
     "Hotine Oblique Mercator (variant B)",
     {
         {"Latitude of projection centre", 6},
         {"Longitude of projection centre", 5},
         {"Azimuth of initial line", 4},
         {"Angle from Rectified to Skew Grid", 7},
         {"Scale factor on initial line", 3},
         {"Easting at projection centre", 1},
         {"Northing at projection centre", 2},
     }},

    {"Goode_Homolosine",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Goode Homolosine",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Equidistant_Cylindrical_Ellipsoidal",
     {
         {"False_Easting", 1},
         {"False_Northing", 2},
         {"Central_Meridian", 3},
         {"Standard_Parallel_1", 4},
     },
     "Equidistant Cylindrical",
     {
         {"Latitude of 1st standard parallel", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Laborde_Oblique_Mercator",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Scale_Factor", 3},
      {"Azimuth", 4},
      {"Longitude_Of_Center", 5},
      {"Latitude_Of_Center", 6}},
     "Laborde Oblique Mercator",
     {
         {"Latitude of projection centre", 6},
         {"Longitude of projection centre", 5},
         {"Azimuth of initial line", 4},
         {"Scale factor on initial line", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Gnomonic_Ellipsoidal",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Longitude_Of_Center", 3},
      {"Latitude_Of_Center", 4}},
     "Gnomonic",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Wagner_IV",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Latitude_Of_Center", 0}},
     "Wagner IV",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Wagner_V",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Wagner V",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Wagner_VII",
     {{"False_Easting", 1}, {"False_Northing", 2}, {"Central_Meridian", 3}},
     "Wagner VII",
     {
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Geostationary_Satellite",
     {
         {"False_Easting", 1},
         {"False_Northing", 2},
         {"Longitude_Of_Center", 3},
         {"Height", 4},
         {"Option", 0.0},
     },
     "Geostationary Satellite (Sweep Y)",
     {
         {"Longitude of natural origin", 3},
         {"Satellite Height", 4},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {"Mercator_Auxiliary_Sphere",
     {{"False_Easting", 1},
      {"False_Northing", 2},
      {"Central_Meridian", 3},
      {"Standard_Parallel_1", 4},
      {"Auxiliary_Sphere_Type", 0}},
     "Popular Visualisation Pseudo Mercator",
     {
         {"Latitude of natural origin", 4},
         {"Longitude of natural origin", 3},
         {"False easting", 1},
         {"False northing", 2},
     }},

    {
        "Unknown_Method",
        {{"False_Easting", 1},
         {"False_Northing", 2},
         {"Longitude_Of_Origin", 3},
         {"Latitude_Of_Origin", 4}},
        "Unknown_Method",
        {{"False_Easting", 1},
         {"False_Northing", 2},
         {"Longitude_Of_Origin", 3},
         {"Latitude_Of_Origin", 4}},
    },

};

TEST(wkt_parse, esri_projcs) {

    for (const auto &projDef : esriProjDefs) {
        std::string wkt("PROJCS[\"unnamed\",GEOGCS[\"GCS_WGS_1984\","
                        "DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_1984\","
                        "6378137.0,298.257223563]],PRIMEM[\"Greenwich\",0.0],"
                        "UNIT[\"Degree\",0.0174532925199433]],PROJECTION[\"");
        wkt += projDef.esriProjectionName;
        wkt += "\"],";
        for (const auto &param : projDef.esriParams) {
            wkt += "PARAMETER[\"";
            wkt += param.first;
            wkt += "\",";
            wkt += toString(param.second);
            wkt += "],";
        }
        wkt += "UNIT[\"Meter\",1.0]]";

        auto obj = WKTParser().createFromWKT(wkt);
        auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
        ASSERT_TRUE(crs != nullptr);

        auto conv = crs->derivingConversion();
        auto method = conv->method();
        EXPECT_EQ(method->nameStr(), projDef.wkt2ProjectionName) << wkt;
        auto values = conv->parameterValues();
        EXPECT_EQ(values.size(), projDef.wkt2Params.size()) << wkt;
        if (values.size() == projDef.wkt2Params.size()) {
            for (size_t i = 0; i < values.size(); i++) {
                const auto &opParamvalue =
                    nn_dynamic_pointer_cast<OperationParameterValue>(values[i]);
                ASSERT_TRUE(opParamvalue);
                const auto &paramName = opParamvalue->parameter()->nameStr();
                const auto &parameterValue = opParamvalue->parameterValue();
                EXPECT_EQ(paramName, projDef.wkt2Params[i].first) << wkt;
                EXPECT_EQ(parameterValue->type(),
                          ParameterValue::Type::MEASURE);
                auto measure = parameterValue->value();
                EXPECT_EQ(measure.value(), projDef.wkt2Params[i].second) << wkt;
            }
        }
    }
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_esri_case_insensitive_names) {
    auto wkt = "PROJCS[\"WGS_1984_UTM_Zone_31N\",GEOGCS[\"GCS_WGS_1984\","
               "DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_1984\",6378137.0,"
               "298.257223563]],PRIMEM[\"Greenwich\",0.0],UNIT[\"Degree\","
               "0.0174532925199433]],PROJECTION[\"transverse_mercator\"],"
               "PARAMETER[\"false_easting\",500000.0],"
               "PARAMETER[\"false_northing\",0.0],"
               "PARAMETER[\"central_meridian\",3.0],"
               "PARAMETER[\"scale_factor\",0.9996],"
               "PARAMETER[\"latitude_of_origin\",0.0],UNIT[\"Meter\",1.0]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    int zone = 0;
    bool north = false;
    EXPECT_TRUE(crs->derivingConversion()->isUTM(zone, north));
    EXPECT_EQ(zone, 31);
    EXPECT_TRUE(north);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_esri_non_expected_param_name) {
    // We try to be lax on parameter names.
    auto wkt =
        "PROJCS[\"WGS_1984_UTM_Zone_31N\",GEOGCS[\"GCS_WGS_1984\","
        "DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_1984\",6378137.0,"
        "298.257223563]],PRIMEM[\"Greenwich\",0.0],UNIT[\"Degree\","
        "0.0174532925199433]],PROJECTION[\"transverse_mercator\"],"
        "PARAMETER[\"false_easting\",500000.0],"
        "PARAMETER[\"false_northing\",0.0],"
        "PARAMETER[\"longitude_of_center\",3.0]," // should be Central_Meridian
        "PARAMETER[\"scale_factor\",0.9996],"
        "PARAMETER[\"latitude_of_origin\",0.0],UNIT[\"Meter\",1.0]]";

    auto obj = WKTParser().createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    int zone = 0;
    bool north = false;
    EXPECT_TRUE(crs->derivingConversion()->isUTM(zone, north));
    EXPECT_EQ(zone, 31);
    EXPECT_TRUE(north);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_esri_krovak_south_west) {
    auto wkt = "PROJCS[\"S-JTSK_Krovak\",GEOGCS[\"GCS_S_JTSK\","
               "DATUM[\"D_S_JTSK\","
               "SPHEROID[\"Bessel_1841\",6377397.155,299.1528128]],"
               "PRIMEM[\"Greenwich\",0.0],UNIT[\"Degree\",0.0174532925199433]],"
               "PROJECTION[\"Krovak\"],PARAMETER[\"False_Easting\",0.0],"
               "PARAMETER[\"False_Northing\",0.0],"
               "PARAMETER[\"Pseudo_Standard_Parallel_1\",78.5],"
               "PARAMETER[\"Scale_Factor\",0.9999],"
               "PARAMETER[\"Azimuth\",30.28813975277778],"
               "PARAMETER[\"Longitude_Of_Center\",24.83333333333333],"
               "PARAMETER[\"Latitude_Of_Center\",49.5],"
               "PARAMETER[\"X_Scale\",1.0],"
               "PARAMETER[\"Y_Scale\",1.0],"
               "PARAMETER[\"XY_Plane_Rotation\",0.0],UNIT[\"Meter\",1.0]]";

    auto obj = WKTParser()
                   .attachDatabaseContext(DatabaseContext::create())
                   .createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->derivingConversion()->method()->nameStr(), "Krovak");

    auto expected_wkt2 =
        "PROJCRS[\"S-JTSK / Krovak\",\n"
        "    BASEGEODCRS[\"S-JTSK\",\n"
        "        DATUM[\"System of the Unified Trigonometrical Cadastral "
        "Network\",\n"
        "            ELLIPSOID[\"Bessel 1841\",6377397.155,299.1528128,\n"
        "                LENGTHUNIT[\"metre\",1]],\n"
        "            ID[\"EPSG\",6156]],\n"
        "        PRIMEM[\"Greenwich\",0,\n"
        "            ANGLEUNIT[\"Degree\",0.0174532925199433]]],\n"
        "    CONVERSION[\"unnamed\",\n"
        "        METHOD[\"Krovak\",\n"
        "            ID[\"EPSG\",9819]],\n"
        "        PARAMETER[\"Latitude of projection centre\",49.5,\n"
        "            ANGLEUNIT[\"Degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8811]],\n"
        "        PARAMETER[\"Longitude of origin\",24.8333333333333,\n"
        "            ANGLEUNIT[\"Degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8833]],\n"
        "        PARAMETER[\"Co-latitude of cone axis\",30.2881397527778,\n"
        "            ANGLEUNIT[\"Degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",1036]],\n"
        "        PARAMETER[\"Latitude of pseudo standard parallel\",78.5,\n"
        "            ANGLEUNIT[\"Degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8818]],\n"
        "        PARAMETER[\"Scale factor on pseudo standard "
        "parallel\",0.9999,\n"
        "            SCALEUNIT[\"unity\",1],\n"
        "            ID[\"EPSG\",8819]],\n"
        "        PARAMETER[\"False easting\",0,\n"
        "            LENGTHUNIT[\"metre\",1],\n"
        "            ID[\"EPSG\",8806]],\n"
        "        PARAMETER[\"False northing\",0,\n"
        "            LENGTHUNIT[\"metre\",1],\n"
        "            ID[\"EPSG\",8807]]],\n"
        "    CS[Cartesian,2],\n"
        "        AXIS[\"southing\",south,\n"
        "            ORDER[1],\n"
        "            LENGTHUNIT[\"metre\",1,\n"
        "                ID[\"EPSG\",9001]]],\n"
        "        AXIS[\"westing\",west,\n"
        "            ORDER[2],\n"
        "            LENGTHUNIT[\"metre\",1,\n"
        "                ID[\"EPSG\",9001]]]]";

    EXPECT_EQ(crs->exportToWKT(WKTFormatter::create().get()), expected_wkt2);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_esri_normalize_unit) {
    auto wkt = "PROJCS[\"Accra_Ghana_Grid\",GEOGCS[\"GCS_Accra\","
               "DATUM[\"D_Accra\",SPHEROID[\"War_Office\",6378300.0,296.0]],"
               "PRIMEM[\"Greenwich\",0.0],UNIT[\"Degree\",0.0174532925199433]],"
               "PROJECTION[\"Transverse_Mercator\"],"
               "PARAMETER[\"False_Easting\",900000.0],"
               "PARAMETER[\"False_Northing\",0.0],"
               "PARAMETER[\"Central_Meridian\",-1.0],"
               "PARAMETER[\"Scale_Factor\",0.99975],"
               "PARAMETER[\"Latitude_Of_Origin\",4.666666666666667],"
               "UNIT[\"Foot_Gold_Coast\",0.3047997101815088]]";

    auto obj = WKTParser()
                   .attachDatabaseContext(DatabaseContext::create())
                   .createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->unit().name(),
              "Gold Coast foot");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_esri_ups_north) {
    auto wkt = "PROJCS[\"UPS_North\",GEOGCS[\"GCS_WGS_1984\","
               "DATUM[\"D_WGS_1984\","
               "SPHEROID[\"WGS_1984\",6378137.0,298.257223563]],"
               "PRIMEM[\"Greenwich\",0.0],UNIT[\"Degree\",0.0174532925199433]],"
               "PROJECTION[\"Stereographic\"],"
               "PARAMETER[\"False_Easting\",2000000.0],"
               "PARAMETER[\"False_Northing\",2000000.0],"
               "PARAMETER[\"Central_Meridian\",0.0],"
               "PARAMETER[\"Scale_Factor\",0.994],"
               "PARAMETER[\"Latitude_Of_Origin\",90.0],"
               "UNIT[\"Meter\",1.0]]";

    auto obj = WKTParser()
                   .attachDatabaseContext(DatabaseContext::create())
                   .createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "WGS 84 / UPS North (E,N)");
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->direction(),
              AxisDirection::SOUTH);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->abbreviation(), "E");
    EXPECT_EQ(crs->coordinateSystem()->axisList()[1]->direction(),
              AxisDirection::SOUTH);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[1]->abbreviation(), "N");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, wkt1_esri_ups_south) {
    auto wkt = "PROJCS[\"UPS_South\",GEOGCS[\"GCS_WGS_1984\","
               "DATUM[\"D_WGS_1984\","
               "SPHEROID[\"WGS_1984\",6378137.0,298.257223563]],"
               "PRIMEM[\"Greenwich\",0.0],UNIT[\"Degree\",0.0174532925199433]],"
               "PROJECTION[\"Stereographic\"],"
               "PARAMETER[\"False_Easting\",2000000.0],"
               "PARAMETER[\"False_Northing\",2000000.0],"
               "PARAMETER[\"Central_Meridian\",0.0],"
               "PARAMETER[\"Scale_Factor\",0.994],"
               "PARAMETER[\"Latitude_Of_Origin\",-90.0],"
               "UNIT[\"Meter\",1.0]]";

    auto obj = WKTParser()
                   .attachDatabaseContext(DatabaseContext::create())
                   .createFromWKT(wkt);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->nameStr(), "WGS 84 / UPS South (E,N)");
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->direction(),
              AxisDirection::NORTH);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[0]->abbreviation(), "E");
    EXPECT_EQ(crs->coordinateSystem()->axisList()[1]->direction(),
              AxisDirection::NORTH);
    EXPECT_EQ(crs->coordinateSystem()->axisList()[1]->abbreviation(), "N");
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid) {
    EXPECT_THROW(WKTParser().createFromWKT(""), ParsingException);
    EXPECT_THROW(WKTParser().createFromWKT("A"), ParsingException);
    EXPECT_THROW(WKTParser().createFromWKT("UNKNOWN[\"foo\"]"),
                 ParsingException);
    EXPECT_THROW(WKTParser().createFromWKT("INVALID["), ParsingException);
    EXPECT_THROW(WKTParser().createFromWKT("INVALID[]"), ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_SPHEROID) {
    EXPECT_NO_THROW(WKTParser().createFromWKT("SPHEROID[\"x\",1,0.5]"));
    EXPECT_THROW(WKTParser().createFromWKT("SPHEROID[\"x\"]"),
                 ParsingException); // major axis not number
    EXPECT_THROW(WKTParser().createFromWKT("SPHEROID[\"x\",\"1\",0.5]"),
                 ParsingException); // major axis not number
    EXPECT_THROW(WKTParser().createFromWKT("SPHEROID[\"x\",1,\"0.5\"]"),
                 ParsingException); // reverse flatting not number
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_DATUM) {
    EXPECT_NO_THROW(
        WKTParser().createFromWKT("DATUM[\"x\",SPHEROID[\"x\",1,0.5]]"));
    EXPECT_THROW(WKTParser().createFromWKT("DATUM[\"x\"]"), ParsingException);
    EXPECT_THROW(WKTParser().createFromWKT("DATUM[\"x\",FOO[]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_ENSEMBLE) {
    EXPECT_THROW(WKTParser().createFromWKT("ENSEMBLE[]"), ParsingException);
    EXPECT_THROW(WKTParser().createFromWKT("ENSEMBLE[\"x\"]"),
                 ParsingException);
    EXPECT_THROW(WKTParser().createFromWKT(
                     "ENSEMBLE[\"x\",MEMBER[\"vdatum1\"],MEMBER[\"vdatum2\"]]"),
                 ParsingException);
    EXPECT_THROW(
        WKTParser().createFromWKT("ENSEMBLE[\"x\",MEMBER[],MEMBER[\"vdatum2\"],"
                                  "ENSEMBLEACCURACY[\"100\"]]"),
        ParsingException);
    EXPECT_THROW(
        WKTParser().createFromWKT("ENSEMBLE[\"x\",MEMBER[\"vdatum1\"],MEMBER["
                                  "\"vdatum2\"],ENSEMBLEACCURACY[]]"),
        ParsingException);
    EXPECT_THROW(
        WKTParser().createFromWKT("ENSEMBLE[\"x\",ENSEMBLEACCURACY[\"100\"]]"),
        ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_GEOGCS) {
    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "GEOGCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0.5]],PRIMEM[\"x\",0],UNIT["
        "\"degree\",0.0174532925199433]]"));

    // missing PRIMEM
    EXPECT_THROW(
        WKTParser().createFromWKT("GEOGCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0."
                                  "5]],UNIT[\"degree\",0.0174532925199433]]"),
        ParsingException);

    // missing UNIT
    EXPECT_THROW(
        WKTParser().createFromWKT(
            "GEOGCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0.5]],PRIMEM[\"x\",0]]"),
        ParsingException);

    EXPECT_THROW(WKTParser().createFromWKT("GEOGCS[\"x\"]"), ParsingException);
    EXPECT_THROW(WKTParser().createFromWKT("GEOGCS[\"x\",FOO[]]"),
                 ParsingException);

    // not enough children for DATUM
    EXPECT_THROW(
        WKTParser().createFromWKT("GEOGCS[\"x\",DATUM[\"x\"],PRIMEM[\"x\",0],"
                                  "UNIT[\"degree\",0.0174532925199433]]"),
        ParsingException);

    // not enough children for AUTHORITY
    EXPECT_THROW(WKTParser().createFromWKT("GEOGCS[\"x\",DATUM[\"x\",SPHEROID["
                                           "\"x\",1,0.5]],PRIMEM[\"x\",0],UNIT["
                                           "\"degree\",0.0174532925199433],"
                                           "AUTHORITY[\"x\"]]"),
                 ParsingException);

    // not enough children for AUTHORITY, but ignored
    EXPECT_NO_THROW(WKTParser().setStrict(false).createFromWKT(
        "GEOGCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0.5]],PRIMEM[\"x\",0],UNIT["
        "\"degree\",0.0174532925199433],AUTHORITY[\"x\"]]"));

    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "GEOGCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0.5]],PRIMEM[\"x\",0],UNIT["
        "\"degree\",0.0174532925199433]]"));

    // PRIMEM not numeric
    EXPECT_THROW(
        WKTParser().createFromWKT(
            "GEOGCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0."
            "5]],PRIMEM[\"x\",\"a\"],UNIT[\"degree\",0.0174532925199433]]"),
        ParsingException);

    // not enough children for PRIMEM
    EXPECT_THROW(WKTParser().createFromWKT("GEOGCS[\"x\",DATUM[\"x\",SPHEROID["
                                           "\"x\",1,0.5]],PRIMEM[\"x\"],UNIT["
                                           "\"degree\",0.0174532925199433]]"),
                 ParsingException);

    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "GEOGCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0.5]],PRIMEM[\"x\",0],UNIT["
        "\"degree\",0.0174532925199433],AXIS[\"latitude\","
        "NORTH],AXIS[\"longitude\",EAST]]"));

    // one axis only
    EXPECT_THROW(WKTParser().createFromWKT(
                     "GEOGCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0."
                     "5]],PRIMEM[\"x\",0],UNIT[\"degree\",0.0174532925199433],"
                     "AXIS[\"latitude\",NORTH]]"),
                 ParsingException);

    // invalid axis
    EXPECT_THROW(WKTParser().createFromWKT("GEOGCS[\"x\",DATUM[\"x\",SPHEROID["
                                           "\"x\",1,0.5]],PRIMEM[\"x\",0],UNIT["
                                           "\"degree\",0.0174532925199433],"
                                           "AXIS[\"latitude\","
                                           "NORTH],AXIS[\"longitude\"]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_UNIT) {
    std::string startWKT("GEODCRS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0.5]],CS["
                         "ellipsoidal,2],AXIS[\"latitude\",north],AXIS["
                         "\"longitude\",east],");

    EXPECT_NO_THROW(WKTParser().createFromWKT(
        startWKT + "UNIT[\"degree\",0.0174532925199433]]"));

    // not enough children
    EXPECT_THROW(WKTParser().createFromWKT(startWKT + "UNIT[\"x\"]]]"),
                 ParsingException);

    // invalid conversion factor
    EXPECT_THROW(
        WKTParser().createFromWKT(startWKT + "UNIT[\"x\",\"invalid\"]]]"),
        ParsingException);

    // invalid ID
    EXPECT_THROW(
        WKTParser().createFromWKT(startWKT + "UNIT[\"x\",1,ID[\"x\"]]]]"),
        ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_GEOCCS) {
    EXPECT_NO_THROW(
        WKTParser().createFromWKT("GEOCCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0."
                                  "5]],PRIMEM[\"x\",0],UNIT[\"metre\",1]]"));

    // missing PRIMEM
    EXPECT_THROW(WKTParser().createFromWKT("GEOCCS[\"x\",DATUM[\"x\",SPHEROID["
                                           "\"x\",1,0.5]],UNIT[\"metre\",1]]"),
                 ParsingException);

    // missing UNIT
    EXPECT_THROW(
        WKTParser().createFromWKT(
            "GEOCCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0.5]],PRIMEM[\"x\",0]]"),
        ParsingException);

    // ellipsoidal CS is invalid in a GEOCCS
    EXPECT_THROW(WKTParser().createFromWKT("GEOCCS[\"x\",DATUM[\"x\",SPHEROID["
                                           "\"x\",1,0.5]],PRIMEM[\"x\",0],UNIT["
                                           "\"degree\",0.0174532925199433],"
                                           "AXIS[\"latitude\","
                                           "NORTH],AXIS[\"longitude\",EAST]]"),
                 ParsingException);

    // 3 axis required
    EXPECT_THROW(WKTParser().createFromWKT(
                     "GEOCCS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0.5]],PRIMEM["
                     "\"x\",0],UNIT[\"metre\",1],AXIS["
                     "\"Geocentric X\",OTHER],AXIS[\"Geocentric Y\",OTHER]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_CS_of_GEODCRS) {

    std::string startWKT("GEODCRS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0.5]]");

    // missing CS
    EXPECT_THROW(WKTParser().createFromWKT(startWKT + "]"), ParsingException);

    // CS: not enough children
    EXPECT_THROW(WKTParser().createFromWKT(startWKT + ",CS[x]]"),
                 ParsingException);

    // CS: invalid type
    EXPECT_THROW(WKTParser().createFromWKT(startWKT + ",CS[x,2]]"),
                 ParsingException);

    // CS: invalid number of axis
    EXPECT_THROW(WKTParser().createFromWKT(startWKT + ",CS[ellipsoidal,0]]"),
                 ParsingException);

    // CS: number of axis is not a number
    EXPECT_THROW(
        WKTParser().createFromWKT(startWKT + ",CS[ellipsoidal,\"x\"]]"),
        ParsingException);

    // CS: invalid CS type
    EXPECT_THROW(WKTParser().createFromWKT(startWKT +
                                           ",CS[invalid,2],AXIS[\"latitude\","
                                           "north],AXIS[\"longitude\",east]]"),
                 ParsingException);

    // CS: OK
    EXPECT_NO_THROW(WKTParser().createFromWKT(
        startWKT + ",CS[ellipsoidal,2],AXIS[\"latitude\",north],AXIS["
                   "\"longitude\",east],UNIT[\"degree\",0.0174532925199433]]"));

    // CS: Cartesian with 2 axis unexpected
    EXPECT_THROW(WKTParser().createFromWKT(
                     startWKT + ",CS[Cartesian,2],AXIS[\"latitude\","
                                "north],AXIS[\"longitude\",east],"
                                "UNIT[\"degree\",0.0174532925199433]]"),
                 ParsingException);

    // CS: missing axis
    EXPECT_THROW(WKTParser().createFromWKT(
                     startWKT + ",CS[ellipsoidal,2],AXIS[\"latitude\",north],"
                                "UNIT[\"degree\",0.0174532925199433]]"),
                 ParsingException);

    // not enough children in AXIS
    EXPECT_THROW(
        WKTParser().createFromWKT(
            startWKT +
            ",CS[ellipsoidal,2],AXIS[\"latitude\",north],AXIS[\"longitude\"],"
            "UNIT[\"degree\",0.0174532925199433]]"),
        ParsingException);

    // not enough children in ORDER
    EXPECT_THROW(WKTParser().createFromWKT(
                     startWKT +
                     ",CS[ellipsoidal,2],AXIS[\"latitude\",north,ORDER[]],AXIS["
                     "\"longitude\",east],"
                     "UNIT[\"degree\",0.0174532925199433]]"),
                 ParsingException);

    // invalid value in ORDER
    EXPECT_THROW(
        WKTParser().createFromWKT(
            startWKT +
            ",CS[ellipsoidal,2],AXIS[\"latitude\",north,ORDER[\"x\"]],AXIS["
            "\"longitude\",east],UNIT[\"degree\",0.0174532925199433]]"),
        ParsingException);

    // unexpected ORDER value
    EXPECT_THROW(
        WKTParser().createFromWKT(
            startWKT +
            ",CS[ellipsoidal,2],AXIS[\"latitude\",north,ORDER[2]],AXIS["
            "\"longitude\",east],UNIT[\"degree\",0.0174532925199433]]"),
        ParsingException);

    // Invalid CS type
    EXPECT_THROW(WKTParser().createFromWKT(
                     startWKT +
                     ",CS[vertical,1],\n"
                     "        AXIS[\"gravity-related height (H)\",up],\n"
                     "        UNIT[\"metre\",1]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_CS_of_GEOGRAPHICCRS) {
    // A GeographicCRS must have an ellipsoidal CS
    EXPECT_THROW(WKTParser().createFromWKT(
                     "GEOGRAPHICCRS[\"x\",DATUM[\"x\",SPHEROID[\"x\",1,0.5]],"
                     "CS[Cartesian,3],AXIS[\"(X)\",geocentricX],AXIS[\"(Y)\","
                     "geocentricY],AXIS[\"(Z)\",geocentricZ]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_DYNAMIC) {
    std::string prefix("GEOGCRS[\"WGS 84 (G1762)\",");
    std::string suffix(
        "TRF[\"World Geodetic System 1984 (G1762)\","
        "ELLIPSOID[\"WGS 84\",6378137,298.257223563]],"
        "CS[ellipsoidal,3],"
        "    AXIS[\"(lat)\",north,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"(lon)\",east,ANGLEUNIT[\"degree\",0.0174532925199433]],"
        "    AXIS[\"ellipsoidal height (h)\",up,LENGTHUNIT[\"metre\",1.0]]"
        "]");

    EXPECT_NO_THROW(WKTParser().createFromWKT(
        prefix + "DYNAMIC[FRAMEEPOCH[2015]]," + suffix));

    EXPECT_THROW(WKTParser().createFromWKT(prefix + "DYNAMIC[]," + suffix),
                 ParsingException);
    EXPECT_THROW(
        WKTParser().createFromWKT(prefix + "DYNAMIC[FRAMEEPOCH[]]," + suffix),
        ParsingException);
    EXPECT_THROW(WKTParser().createFromWKT(
                     prefix + "DYNAMIC[FRAMEEPOCH[\"invalid\"]]," + suffix),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_PROJCRS) {
    // missing BASEGEODCRS
    EXPECT_THROW(
        WKTParser().createFromWKT("PROJCRS[\"WGS 84 / UTM zone 31N\"]"),
        ParsingException);

    std::string startWKT("PROJCRS[\"WGS 84 / UTM zone 31N\",BASEGEOGCRS[\"WGS "
                         "84\",DATUM[\"WGS_1984\",ELLIPSOID[\"WGS "
                         "84\",6378137,298.257223563]],UNIT[\"degree\",0."
                         "0174532925199433]]");

    // missing CONVERSION
    EXPECT_THROW(WKTParser().createFromWKT(startWKT + "]"), ParsingException);

    // not enough children in CONVERSION
    EXPECT_THROW(WKTParser().createFromWKT(startWKT + ",CONVERSION[\"x\"]]"),
                 ParsingException);

    // not enough children in METHOD
    EXPECT_THROW(
        WKTParser().createFromWKT(startWKT + ",CONVERSION[\"x\",METHOD[]]]"),
        ParsingException);

    // not enough children in PARAMETER
    EXPECT_THROW(
        WKTParser().createFromWKT(
            startWKT + ",CONVERSION[\"x\",METHOD[\"y\"],PARAMETER[\"z\"]]]"),
        ParsingException);

    // non numeric value for PARAMETER
    EXPECT_THROW(
        WKTParser().createFromWKT(
            startWKT +
            ",CONVERSION[\"x\",METHOD[\"y\"],PARAMETER[\"z\",\"foo\"]]]"),
        ParsingException);

    // missing CS
    EXPECT_THROW(WKTParser().createFromWKT(startWKT +
                                           ",CONVERSION[\"x\",METHOD[\"y\"]]]"),
                 ParsingException);

    // CS is not Cartesian
    EXPECT_THROW(WKTParser().createFromWKT(
                     startWKT + ",CONVERSION[\"x\",METHOD[\"y\"]],CS["
                                "ellipsoidal,2],AXIS[\"latitude\",north],AXIS["
                                "\"longitude\",east]]"),
                 ParsingException);

    // not enough children in MERIDIAN
    EXPECT_THROW(WKTParser().createFromWKT(
                     startWKT + ",CONVERSION[\"x\",METHOD[\"y\"]],CS["
                                "Cartesian,2],AXIS[\"easting (X)\",south,"
                                "MERIDIAN[90]],AXIS["
                                "\"northing (Y)\",south]]"),
                 ParsingException);

    // non numeric angle value for MERIDIAN
    EXPECT_THROW(
        WKTParser().createFromWKT(
            startWKT +
            ",CONVERSION[\"x\",METHOD[\"y\"]],CS["
            "Cartesian,2],AXIS[\"easting (X)\",south,"
            "MERIDIAN[\"x\",UNIT[\"degree\",0.0174532925199433]]],AXIS["
            "\"northing (Y)\",south]]"),
        ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_PROJCS) {

    std::string startWKT(
        "PROJCS[\"WGS 84 / UTM zone 31N\",\n"
        "    GEOGCS[\"WGS 84\",\n"
        "        DATUM[\"WGS_1984\",\n"
        "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
        "                AUTHORITY[\"EPSG\",\"7030\"]],\n"
        "            AUTHORITY[\"EPSG\",\"6326\"]],\n"
        "        PRIMEM[\"x\",0],\n"
        "        UNIT[\"degree\",0.0174532925199433,\n"
        "            AUTHORITY[\"EPSG\",\"9122\"]],\n"
        "        AXIS[\"latitude\",NORTH],\n"
        "        AXIS[\"longitude\",EAST],\n"
        "        AUTHORITY[\"EPSG\",\"4326\"]]\n");

    // missing PROJECTION
    EXPECT_THROW(WKTParser().createFromWKT(startWKT + "]"), ParsingException);

    // not enough children in PROJECTION
    EXPECT_THROW(WKTParser().createFromWKT(startWKT +
                                           ",PROJECTION[],UNIT[\"metre\",1]]"),
                 ParsingException);

    // not enough children in PARAMETER
    EXPECT_THROW(WKTParser().createFromWKT(
                     startWKT +
                     ",PROJECTION[\"x\"],PARAMETER[\"z\"],UNIT[\"metre\",1]]"),
                 ParsingException);

    // not enough children in PARAMETER
    EXPECT_THROW(WKTParser().createFromWKT(
                     startWKT +
                     ",PROJECTION[\"x\"],PARAMETER[\"z\"],UNIT[\"metre\",1]]"),
                 ParsingException);

    EXPECT_NO_THROW(WKTParser().createFromWKT(
        startWKT + ",PROJECTION[\"x\"],PARAMETER[\"z\",1],UNIT[\"metre\",1]]"));

    // missing UNIT
    EXPECT_THROW(WKTParser().createFromWKT(
                     startWKT + ",PROJECTION[\"x\"],PARAMETER[\"z\",1]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_VERTCRS) {

    // missing VDATUM
    EXPECT_THROW(WKTParser().createFromWKT(
                     "VERTCRS[\"foo\",CS[vertical,1],AXIS[\"x\",up]]"),
                 ParsingException);

    // missing CS
    EXPECT_THROW(WKTParser().createFromWKT("VERTCRS[\"foo\",VDATUM[\"bar\"]]"),
                 ParsingException);

    // CS is not of type vertical
    EXPECT_THROW(WKTParser().createFromWKT("VERTCRS[\"foo\",VDATUM[\"bar\"],CS["
                                           "ellipsoidal,2],AXIS[\"latitude\","
                                           "north],AXIS["
                                           "\"longitude\",east]]"),
                 ParsingException);

    // verticalCS should have only 1 axis
    EXPECT_THROW(
        WKTParser().createFromWKT("VERTCRS[\"foo\",VDATUM[\"bar\"],CS[vertical,"
                                  "2],AXIS[\"latitude\",north],AXIS["
                                  "\"longitude\",east]]"),
        ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_VERT_CS) {

    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "VERT_CS[\"x\",VERT_DATUM[\"y\",2005],UNIT[\"metre\",1]]"));

    // Missing VERT_DATUM
    EXPECT_THROW(WKTParser().createFromWKT("VERT_CS[\"x\",UNIT[\"metre\",1]]"),
                 ParsingException);

    // Missing UNIT
    EXPECT_THROW(
        WKTParser().createFromWKT("VERT_CS[\"x\",VERT_DATUM[\"y\",2005]]"),
        ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_COORDINATEOPERATION) {

    std::string src_wkt;
    {
        auto formatter = WKTFormatter::create();
        formatter->setOutputId(false);
        src_wkt = GeographicCRS::EPSG_4326->exportToWKT(formatter.get());
    }

    std::string dst_wkt;
    {
        auto formatter = WKTFormatter::create();
        formatter->setOutputId(false);
        dst_wkt = GeographicCRS::EPSG_4807->exportToWKT(formatter.get());
    }

    std::string interpolation_wkt;
    {
        auto formatter = WKTFormatter::create();
        formatter->setOutputId(false);
        interpolation_wkt =
            GeographicCRS::EPSG_4979->exportToWKT(formatter.get());
    }

    // Valid
    {
        auto wkt = "COORDINATEOPERATION[\"transformationName\",\n"
                   "    SOURCECRS[" +
                   src_wkt + "],\n"
                             "    TARGETCRS[" +
                   dst_wkt + "],\n"
                             "    METHOD[\"operationMethodName\"],\n"
                             "    PARAMETERFILE[\"paramName\",\"foo.bin\"]]";

        EXPECT_NO_THROW(WKTParser().createFromWKT(wkt));
    }

    // Missing SOURCECRS
    {
        auto wkt = "COORDINATEOPERATION[\"transformationName\",\n"
                   "    TARGETCRS[" +
                   dst_wkt + "],\n"
                             "    METHOD[\"operationMethodName\"],\n"
                             "    PARAMETERFILE[\"paramName\",\"foo.bin\"]]";

        EXPECT_THROW(WKTParser().createFromWKT(wkt), ParsingException);
    }

    // Invalid content in SOURCECRS
    {
        auto wkt = "COORDINATEOPERATION[\"transformationName\",\n"
                   "    SOURCECRS[FOO],\n"
                   "    TARGETCRS[" +
                   dst_wkt + "],\n"
                             "    METHOD[\"operationMethodName\"],\n"
                             "    PARAMETERFILE[\"paramName\",\"foo.bin\"]]";

        EXPECT_THROW(WKTParser().createFromWKT(wkt), ParsingException);
    }

    // Missing TARGETCRS
    {
        auto wkt = "COORDINATEOPERATION[\"transformationName\",\n"
                   "    SOURCECRS[" +
                   src_wkt + "],\n"
                             "    METHOD[\"operationMethodName\"],\n"
                             "    PARAMETERFILE[\"paramName\",\"foo.bin\"]]";

        EXPECT_THROW(WKTParser().createFromWKT(wkt), ParsingException);
    }

    // Invalid content in TARGETCRS
    {
        auto wkt = "COORDINATEOPERATION[\"transformationName\",\n"
                   "    SOURCECRS[" +
                   src_wkt + "],\n"
                             "    TARGETCRS[FOO],\n"
                             "    METHOD[\"operationMethodName\"],\n"
                             "    PARAMETERFILE[\"paramName\",\"foo.bin\"]]";

        EXPECT_THROW(WKTParser().createFromWKT(wkt), ParsingException);
    }

    // Missing METHOD
    {
        auto wkt = "COORDINATEOPERATION[\"transformationName\",\n"
                   "    SOURCECRS[" +
                   src_wkt + "],\n"
                             "    TARGETCRS[" +
                   dst_wkt + "]]";

        EXPECT_THROW(WKTParser().createFromWKT(wkt), ParsingException);
    }

    // Invalid METHOD
    {
        auto wkt = "COORDINATEOPERATION[\"transformationName\",\n"
                   "    SOURCECRS[" +
                   src_wkt + "],\n"
                             "    TARGETCRS[" +
                   dst_wkt + "],\n"
                             "    METHOD[],\n"
                             "    PARAMETERFILE[\"paramName\",\"foo.bin\"]]";

        EXPECT_THROW(WKTParser().createFromWKT(wkt), ParsingException);
    }
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_CONCATENATEDOPERATION) {

    // No STEP
    EXPECT_THROW(WKTParser().createFromWKT("CONCATENATEDOPERATION[\"name\"]"),
                 ParsingException);

    auto transf_1 = Transformation::create(
        PropertyMap().set(IdentifiedObject::NAME_KEY, "transf_1"),
        nn_static_pointer_cast<CRS>(GeographicCRS::EPSG_4326),
        nn_static_pointer_cast<CRS>(GeographicCRS::EPSG_4807), nullptr,
        PropertyMap().set(IdentifiedObject::NAME_KEY, "operationMethodName"),
        std::vector<OperationParameterNNPtr>{OperationParameter::create(
            PropertyMap().set(IdentifiedObject::NAME_KEY, "paramName"))},
        std::vector<ParameterValueNNPtr>{
            ParameterValue::createFilename("foo.bin")},
        std::vector<PositionalAccuracyNNPtr>());

    // One single STEP
    {
        auto wkt =
            "CONCATENATEDOPERATION[\"name\",\n"
            "    SOURCECRS[" +
            transf_1->sourceCRS()->exportToWKT(WKTFormatter::create().get()) +
            "],\n"
            "    TARGETCRS[" +
            transf_1->targetCRS()->exportToWKT(WKTFormatter::create().get()) +
            "],\n"
            "    STEP[" +
            transf_1->exportToWKT(WKTFormatter::create().get()) +
            "],\n"
            "    ID[\"codeSpace\",\"code\"],\n"
            "    REMARK[\"my remarks\"]]";

        EXPECT_THROW(WKTParser().createFromWKT(wkt), ParsingException);
    }

    // empty STEP
    {
        auto wkt =
            "CONCATENATEDOPERATION[\"name\",\n"
            "    SOURCECRS[" +
            transf_1->sourceCRS()->exportToWKT(WKTFormatter::create().get()) +
            "],\n"
            "    TARGETCRS[" +
            transf_1->targetCRS()->exportToWKT(WKTFormatter::create().get()) +
            "],\n"
            "    STEP[],\n"
            "    STEP[],\n"
            "    ID[\"codeSpace\",\"code\"],\n"
            "    REMARK[\"my remarks\"]]";
        EXPECT_THROW(WKTParser().createFromWKT(wkt), ParsingException);
    }

    // Invalid content in STEP
    {
        auto wkt =
            "CONCATENATEDOPERATION[\"name\",\n"
            "    SOURCECRS[" +
            transf_1->sourceCRS()->exportToWKT(WKTFormatter::create().get()) +
            "],\n"
            "    TARGETCRS[" +
            transf_1->targetCRS()->exportToWKT(WKTFormatter::create().get()) +
            "],\n"
            "    STEP[" +
            transf_1->sourceCRS()->exportToWKT(WKTFormatter::create().get()) +
            "],\n"
            "    STEP[" +
            transf_1->sourceCRS()->exportToWKT(WKTFormatter::create().get()) +
            "],\n"
            "    ID[\"codeSpace\",\"code\"],\n"
            "    REMARK[\"my remarks\"]]";

        EXPECT_THROW(WKTParser().createFromWKT(wkt), ParsingException);
    }
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_BOUNDCRS) {

    auto projcrs = ProjectedCRS::create(
        PropertyMap().set(IdentifiedObject::NAME_KEY, "my PROJCRS"),
        GeographicCRS::create(
            PropertyMap().set(IdentifiedObject::NAME_KEY, "my GEOGCRS"),
            GeodeticReferenceFrame::EPSG_6326,
            EllipsoidalCS::createLatitudeLongitude(UnitOfMeasure::DEGREE)),
        Conversion::createUTM(PropertyMap(), 31, true),
        CartesianCS::createEastingNorthing(UnitOfMeasure::METRE));

    auto valid_wkt =
        "BOUNDCRS[SOURCECRS[" +
        projcrs->exportToWKT(WKTFormatter::create().get()) + "],\n" +
        "TARGETCRS[" +
        GeographicCRS::EPSG_4326->exportToWKT(WKTFormatter::create().get()) +
        "],\n"
        "    ABRIDGEDTRANSFORMATION[\"foo\",\n"
        "        METHOD[\"bar\"],PARAMETER[\"foo\",1.0]]]";
    EXPECT_NO_THROW(WKTParser().createFromWKT(valid_wkt)) << valid_wkt;

    // Missing SOURCECRS
    EXPECT_THROW(
        WKTParser().createFromWKT("BOUNDCRS[TARGETCRS[" +
                                  GeographicCRS::EPSG_4326->exportToWKT(
                                      WKTFormatter::create().get()) +
                                  "],\n"
                                  "    ABRIDGEDTRANSFORMATION[\"foo\",\n"
                                  "        METHOD[\"bar\"],"
                                  "PARAMETER[\"foo\",1.0]]]"),
        ParsingException);

    // Invalid SOURCECRS
    EXPECT_THROW(
        WKTParser().createFromWKT("BOUNDCRS[SOURCECRS[foo], TARGETCRS[" +
                                  GeographicCRS::EPSG_4326->exportToWKT(
                                      WKTFormatter::create().get()) +
                                  "],\n"
                                  "    ABRIDGEDTRANSFORMATION[\"foo\",\n"
                                  "        METHOD[\"bar\"],"
                                  "PARAMETER[\"foo\",1.0]]]"),
        ParsingException);

    // Missing TARGETCRS
    EXPECT_THROW(WKTParser().createFromWKT(
                     "BOUNDCRS[SOURCECRS[" +
                     projcrs->exportToWKT(WKTFormatter::create().get()) +
                     "],\n"
                     "    ABRIDGEDTRANSFORMATION[\"foo\",\n"
                     "        METHOD[\"bar\"],"
                     "PARAMETER[\"foo\",1.0]]]"),
                 ParsingException);

    // Invalid TARGETCRS
    EXPECT_THROW(WKTParser().createFromWKT(
                     "BOUNDCRS[SOURCECRS[" +
                     projcrs->exportToWKT(WKTFormatter::create().get()) +
                     "],TARGETCRS[\"foo\"],\n"
                     "    ABRIDGEDTRANSFORMATION[\"foo\",\n"
                     "        METHOD[\"bar\"],"
                     "PARAMETER[\"foo\",1.0]]]"),
                 ParsingException);

    // Missing ABRIDGEDTRANSFORMATION
    EXPECT_THROW(WKTParser().createFromWKT(
                     "BOUNDCRS[SOURCECRS[" +
                     projcrs->exportToWKT(WKTFormatter::create().get()) +
                     "],\n" + "TARGETCRS[" +
                     GeographicCRS::EPSG_4326->exportToWKT(
                         WKTFormatter::create().get()) +
                     "]]"),
                 ParsingException);

    // Missing METHOD
    EXPECT_THROW(WKTParser().createFromWKT(
                     "BOUNDCRS[SOURCECRS[" +
                     projcrs->exportToWKT(WKTFormatter::create().get()) +
                     "],\n" + "TARGETCRS[" +
                     GeographicCRS::EPSG_4326->exportToWKT(
                         WKTFormatter::create().get()) +
                     "],"
                     "ABRIDGEDTRANSFORMATION[\"foo\"],"
                     "PARAMETER[\"foo\",1.0]]"),
                 ParsingException);

    // Invalid METHOD
    EXPECT_THROW(WKTParser().createFromWKT(
                     "BOUNDCRS[SOURCECRS[" +
                     projcrs->exportToWKT(WKTFormatter::create().get()) +
                     "],\n" + "TARGETCRS[" +
                     GeographicCRS::EPSG_4326->exportToWKT(
                         WKTFormatter::create().get()) +
                     "],"
                     "ABRIDGEDTRANSFORMATION[\"foo\",METHOD[],"
                     "PARAMETER[\"foo\",1.0]]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_TOWGS84) {
    EXPECT_THROW(WKTParser().createFromWKT(
                     "GEOGCS[\"WGS 84\","
                     "    DATUM[\"WGS_1984\","
                     "        SPHEROID[\"WGS 84\",6378137,298.257223563],"
                     "        TOWGS84[0]],"
                     "    PRIMEM[\"Greenwich\",0],"
                     "    UNIT[\"degree\",0.0174532925199433]]"),
                 ParsingException);

    EXPECT_THROW(WKTParser().createFromWKT(
                     "GEOGCS[\"WGS 84\","
                     "    DATUM[\"WGS_1984\","
                     "        SPHEROID[\"WGS 84\",6378137,298.257223563],"
                     "        TOWGS84[0,0,0,0,0,0,\"foo\"]],"
                     "    PRIMEM[\"Greenwich\",0],"
                     "    UNIT[\"degree\",0.0174532925199433]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_DerivedGeographicCRS) {

    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "GEODCRS[\"WMO Atlantic Pole\",\n"
        "    BASEGEODCRS[\"WGS 84\",\n"
        "        DATUM[\"World Geodetic System 1984\",\n"
        "            ELLIPSOID[\"WGS 84\",6378137,298.257223563]]],\n"
        "    DERIVINGCONVERSION[\"foo\",\n"
        "        METHOD[\"bar\"],\n"
        "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
        "    CS[ellipsoidal,2],\n"
        "        AXIS[\"latitude\",north],\n"
        "        AXIS[\"longitude\",east],\n"
        "        UNIT[\"degree\",0.0174532925199433]]"));

    // Missing DERIVINGCONVERSION
    EXPECT_THROW(
        WKTParser().createFromWKT(
            "GEODCRS[\"WMO Atlantic Pole\",\n"
            "    BASEGEODCRS[\"WGS 84\",\n"
            "        DATUM[\"World Geodetic System 1984\",\n"
            "            ELLIPSOID[\"WGS 84\",6378137,298.257223563]]],\n"
            "    CS[ellipsoidal,2],\n"
            "        AXIS[\"latitude\",north],\n"
            "        AXIS[\"longitude\",east],\n"
            "        UNIT[\"degree\",0.0174532925199433]]"),
        ParsingException);

    // Missing CS
    EXPECT_THROW(
        WKTParser().createFromWKT(
            "GEODCRS[\"WMO Atlantic Pole\",\n"
            "    BASEGEODCRS[\"WGS 84\",\n"
            "        DATUM[\"World Geodetic System 1984\",\n"
            "            ELLIPSOID[\"WGS 84\",6378137,298.257223563]]],\n"
            "    DERIVINGCONVERSION[\"foo\",\n"
            "        METHOD[\"bar\"],\n"
            "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]]]"),
        ParsingException);

    // CS should be ellipsoidal given root node is GEOGCRS
    EXPECT_THROW(
        WKTParser().createFromWKT(
            "GEOGCRS[\"WMO Atlantic Pole\",\n"
            "    BASEGEOGCRS[\"WGS 84\",\n"
            "        DATUM[\"World Geodetic System 1984\",\n"
            "            ELLIPSOID[\"WGS 84\",6378137,298.257223563]]],\n"
            "    DERIVINGCONVERSION[\"foo\",\n"
            "        METHOD[\"bar\"],\n"
            "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
            "    CS[Cartesian,3],\n"
            "        AXIS[\"(X)\",geocentricX],\n"
            "        AXIS[\"(Y)\",geocentricY],\n"
            "        AXIS[\"(Z)\",geocentricZ],\n"
            "        UNIT[\"metre\",1]]"),
        ParsingException);

    // CS should have 3 axis
    EXPECT_THROW(
        WKTParser().createFromWKT(
            "GEODCRS[\"WMO Atlantic Pole\",\n"
            "    BASEGEODCRS[\"WGS 84\",\n"
            "        DATUM[\"World Geodetic System 1984\",\n"
            "            ELLIPSOID[\"WGS 84\",6378137,298.257223563]]],\n"
            "    DERIVINGCONVERSION[\"foo\",\n"
            "        METHOD[\"bar\"],\n"
            "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
            "    CS[Cartesian,2],\n"
            "        AXIS[\"(X)\",geocentricX],\n"
            "        AXIS[\"(Y)\",geocentricY],\n"
            "        UNIT[\"metre\",1]]"),
        ParsingException);

    // Invalid CS type
    EXPECT_THROW(
        WKTParser().createFromWKT(
            "GEODCRS[\"WMO Atlantic Pole\",\n"
            "    BASEGEODCRS[\"WGS 84\",\n"
            "        DATUM[\"World Geodetic System 1984\",\n"
            "            ELLIPSOID[\"WGS 84\",6378137,298.257223563]]],\n"
            "    DERIVINGCONVERSION[\"foo\",\n"
            "        METHOD[\"bar\"],\n"
            "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
            "    CS[vertical,1],\n"
            "        AXIS[\"gravity-related height (H)\",up],\n"
            "        UNIT[\"metre\",1]]"),
        ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_TemporalCRS) {

    EXPECT_NO_THROW(
        WKTParser().createFromWKT("TIMECRS[\"Temporal CRS\",\n"
                                  "    TDATUM[\"Gregorian calendar\",\n"
                                  "        TIMEORIGIN[0000-01-01]],\n"
                                  "    CS[temporal,1],\n"
                                  "        AXIS[\"time (T)\",future]]"));

    // Missing TDATUM
    EXPECT_THROW(
        WKTParser().createFromWKT("TIMECRS[\"Temporal CRS\",\n"
                                  "    CS[temporal,1],\n"
                                  "        AXIS[\"time (T)\",future]]"),
        ParsingException);

    // Missing CS
    EXPECT_THROW(
        WKTParser().createFromWKT("TIMECRS[\"Temporal CRS\",\n"
                                  "    TDATUM[\"Gregorian calendar\",\n"
                                  "        TIMEORIGIN[0000-01-01]]]"),
        ParsingException);

    // CS should be temporal
    EXPECT_THROW(
        WKTParser().createFromWKT("TIMECRS[\"Temporal CRS\",\n"
                                  "    TDATUM[\"Gregorian calendar\",\n"
                                  "        TIMEORIGIN[0000-01-01]],\n"
                                  "    CS[Cartesian,2],\n"
                                  "        AXIS[\"(X)\",geocentricX],\n"
                                  "        AXIS[\"(Y)\",geocentricY],\n"
                                  "        UNIT[\"metre\",1]]"),
        ParsingException);

    // CS should have 1 axis
    EXPECT_THROW(
        WKTParser().createFromWKT("TIMECRS[\"Temporal CRS\",\n"
                                  "    TDATUM[\"Gregorian calendar\",\n"
                                  "        TIMEORIGIN[0000-01-01]],\n"
                                  "    CS[temporal,2],\n"
                                  "        AXIS[\"time (T)\",future],\n"
                                  "        AXIS[\"time2 (T)\",future]]"),
        ParsingException);

    // CS should have 1 axis
    EXPECT_THROW(
        WKTParser().createFromWKT("TIMECRS[\"Temporal CRS\",\n"
                                  "    TDATUM[\"Gregorian calendar\",\n"
                                  "        TIMEORIGIN[0000-01-01]],\n"
                                  "    CS[TemporalDateTime,2],\n"
                                  "        AXIS[\"time (T)\",future],\n"
                                  "        AXIS[\"time2 (T)\",future]]"),
        ParsingException);

    // CS should have 1 axis
    EXPECT_THROW(
        WKTParser().createFromWKT("TIMECRS[\"Temporal CRS\",\n"
                                  "    TDATUM[\"Gregorian calendar\",\n"
                                  "        TIMEORIGIN[0000-01-01]],\n"
                                  "    CS[TemporalCount,2],\n"
                                  "        AXIS[\"time (T)\",future],\n"
                                  "        AXIS[\"time2 (T)\",future]]"),
        ParsingException);

    // CS should have 1 axis
    EXPECT_THROW(
        WKTParser().createFromWKT("TIMECRS[\"Temporal CRS\",\n"
                                  "    TDATUM[\"Gregorian calendar\",\n"
                                  "        TIMEORIGIN[0000-01-01]],\n"
                                  "    CS[TemporalMeasure,2],\n"
                                  "        AXIS[\"time (T)\",future],\n"
                                  "        AXIS[\"time2 (T)\",future]]"),
        ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_EngineeingCRS) {

    EXPECT_NO_THROW(
        WKTParser().createFromWKT("ENGCRS[\"name\",\n"
                                  "    EDATUM[\"name\"],\n"
                                  "    CS[temporal,1],\n"
                                  "        AXIS[\"time (T)\",future]]"));

    // Missing EDATUM
    EXPECT_THROW(
        WKTParser().createFromWKT("ENGCRS[\"name\",\n"
                                  "    CS[temporal,1],\n"
                                  "        AXIS[\"time (T)\",future]]"),
        ParsingException);

    // Missing CS
    EXPECT_THROW(WKTParser().createFromWKT("ENGCRS[\"name\",\n"
                                           "    EDATUM[\"name\"]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_LOCAL_CS) {

    EXPECT_THROW(
        WKTParser().createFromWKT("LOCAL_CS[\"name\",\n"
                                  "  LOCAL_DATUM[\"name\",1234],\n"
                                  "  AXIS[\"Geodetic latitude\",NORTH],\n"
                                  "  AXIS[\"Geodetic longitude\",EAST],\n"
                                  "  AXIS[\"Ellipsoidal height\",UP]]"),
        ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_ParametricCRS) {

    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "PARAMETRICCRS[\"name\",\n"
        "    PDATUM[\"name\"],\n"
        "    CS[parametric,1],\n"
        "        AXIS[\"pressure (hPa)\",up,\n"
        "            PARAMETRICUNIT[\"HectoPascal\",100]]]"));

    // Missing PDATUM
    EXPECT_THROW(WKTParser().createFromWKT(
                     "PARAMETRICCRS[\"name\",\n"
                     "    CS[parametric,1],\n"
                     "        AXIS[\"pressure (hPa)\",up,\n"
                     "            PARAMETRICUNIT[\"HectoPascal\",100]]]"),
                 ParsingException);

    // Missing CS
    EXPECT_THROW(WKTParser().createFromWKT("PARAMETRICCRS[\"name\",\n"
                                           "    PDATUM[\"name\"]]"),
                 ParsingException);

    // Invalid number of axis for CS
    EXPECT_THROW(WKTParser().createFromWKT(
                     "PARAMETRICCRS[\"name\",\n"
                     "    PDATUM[\"name\"],\n"
                     "    CS[parametric,2],\n"
                     "        AXIS[\"pressure (hPa)\",up,\n"
                     "            PARAMETRICUNIT[\"HectoPascal\",100]]"
                     "        AXIS[\"pressure (hPa)\",up,\n"
                     "            PARAMETRICUNIT[\"HectoPascal\",100]]]"),
                 ParsingException);

    // Invalid CS type
    EXPECT_THROW(
        WKTParser().createFromWKT("PARAMETRICCRS[\"name\",\n"
                                  "    PDATUM[\"name\"],\n"
                                  "    CS[temporal,1],\n"
                                  "        AXIS[\"time (T)\",future]]"),
        ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_DERIVEDPROJCRS) {
    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "DERIVEDPROJCRS[\"derived projectedCRS\",\n"
        "    BASEPROJCRS[\"BASEPROJCRS\",\n"
        "        BASEGEOGCRS[\"WGS 84\",\n"
        "            DATUM[\"World Geodetic System 1984\",\n"
        "                ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                    LENGTHUNIT[\"metre\",1]]]],\n"
        "        CONVERSION[\"unnamed\",\n"
        "            METHOD[\"PROJ unimplemented\"],\n"
        "            PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]]],\n"
        "    DERIVINGCONVERSION[\"unnamed\",\n"
        "        METHOD[\"PROJ unimplemented\"],\n"
        "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
        "    CS[Cartesian,2],\n"
        "        AXIS[\"(E)\",east],\n"
        "        AXIS[\"(N)\",north],\n"
        "        UNIT[\"metre\",1]]"));

    EXPECT_THROW(WKTParser().createFromWKT(
                     "DERIVEDPROJCRS[\"derived projectedCRS\",\n"
                     "    DERIVINGCONVERSION[\"unnamed\",\n"
                     "        METHOD[\"PROJ unimplemented\"],\n"
                     "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
                     "    CS[Cartesian,2],\n"
                     "        AXIS[\"(E)\",east],\n"
                     "        AXIS[\"(N)\",north],\n"
                     "        UNIT[\"metre\",1]]"),
                 ParsingException);

    // Missing DERIVINGCONVERSION
    EXPECT_THROW(
        WKTParser().createFromWKT(
            "DERIVEDPROJCRS[\"derived projectedCRS\",\n"
            "    BASEPROJCRS[\"BASEPROJCRS\",\n"
            "        BASEGEOGCRS[\"WGS 84\",\n"
            "            DATUM[\"World Geodetic System 1984\",\n"
            "                ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
            "                    LENGTHUNIT[\"metre\",1]]]],\n"
            "        CONVERSION[\"unnamed\",\n"
            "            METHOD[\"PROJ unimplemented\"],\n"
            "            PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]]],\n"
            "    CS[Cartesian,2],\n"
            "        AXIS[\"(E)\",east],\n"
            "        AXIS[\"(N)\",north],\n"
            "        UNIT[\"metre\",1]]"),
        ParsingException);

    // Missing CS
    EXPECT_THROW(
        WKTParser().createFromWKT(
            "DERIVEDPROJCRS[\"derived projectedCRS\",\n"
            "    BASEPROJCRS[\"BASEPROJCRS\",\n"
            "        BASEGEOGCRS[\"WGS 84\",\n"
            "            DATUM[\"World Geodetic System 1984\",\n"
            "                ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
            "                    LENGTHUNIT[\"metre\",1]]]],\n"
            "        CONVERSION[\"unnamed\",\n"
            "            METHOD[\"PROJ unimplemented\"],\n"
            "            PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]]],\n"
            "    DERIVINGCONVERSION[\"unnamed\",\n"
            "        METHOD[\"PROJ unimplemented\"],\n"
            "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]]]"),
        ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_DerivedVerticalCRS) {
    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "VERTCRS[\"Derived vertCRS\",\n"
        "    BASEVERTCRS[\"ODN height\",\n"
        "        VDATUM[\"Ordnance Datum Newlyn\"]],\n"
        "    DERIVINGCONVERSION[\"unnamed\",\n"
        "        METHOD[\"PROJ unimplemented\"],\n"
        "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
        "    CS[vertical,1],\n"
        "        AXIS[\"gravity-related height (H)\",up],\n"
        "        UNIT[\"metre\",1]]"));

    // Missing DERIVINGCONVERSION
    EXPECT_THROW(WKTParser().createFromWKT(
                     "VERTCRS[\"Derived vertCRS\",\n"
                     "    BASEVERTCRS[\"ODN height\",\n"
                     "        VDATUM[\"Ordnance Datum Newlyn\"]],\n"
                     "    CS[vertical,1],\n"
                     "        AXIS[\"gravity-related height (H)\",up],\n"
                     "        UNIT[\"metre\",1]]"),
                 ParsingException);

    // Missing CS
    EXPECT_THROW(WKTParser().createFromWKT(
                     "VERTCRS[\"Derived vertCRS\",\n"
                     "    BASEVERTCRS[\"ODN height\",\n"
                     "        VDATUM[\"Ordnance Datum Newlyn\"]],\n"
                     "    DERIVINGCONVERSION[\"unnamed\",\n"
                     "        METHOD[\"PROJ unimplemented\"],\n"
                     "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]]]"),
                 ParsingException);

    // Wrong CS type
    EXPECT_THROW(WKTParser().createFromWKT(
                     "VERTCRS[\"Derived vertCRS\",\n"
                     "    BASEVERTCRS[\"ODN height\",\n"
                     "        VDATUM[\"Ordnance Datum Newlyn\"]],\n"
                     "    DERIVINGCONVERSION[\"unnamed\",\n"
                     "        METHOD[\"PROJ unimplemented\"],\n"
                     "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
                     "    CS[parametric,1],\n"
                     "        AXIS[\"gravity-related height (H)\",up],\n"
                     "        UNIT[\"metre\",1]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_DerivedEngineeringCRS) {
    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "ENGCRS[\"Derived EngineeringCRS\",\n"
        "    BASEENGCRS[\"Engineering CRS\",\n"
        "        EDATUM[\"Engineering datum\"]],\n"
        "    DERIVINGCONVERSION[\"unnamed\",\n"
        "        METHOD[\"PROJ unimplemented\"],\n"
        "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
        "    CS[Cartesian,2],\n"
        "        AXIS[\"(E)\",east],\n"
        "        AXIS[\"(N)\",north],\n"
        "        LENGTHUNIT[\"metre\",1]]"));

    // Missing DERIVINGCONVERSION
    EXPECT_THROW(
        WKTParser().createFromWKT("ENGCRS[\"Derived EngineeringCRS\",\n"
                                  "    BASEENGCRS[\"Engineering CRS\",\n"
                                  "        EDATUM[\"Engineering datum\"]],\n"
                                  "    CS[Cartesian,2],\n"
                                  "        AXIS[\"(E)\",east],\n"
                                  "        AXIS[\"(N)\",north],\n"
                                  "        LENGTHUNIT[\"metre\",1]]"),
        ParsingException);

    // Missing CS
    EXPECT_THROW(WKTParser().createFromWKT(
                     "ENGCRS[\"Derived EngineeringCRS\",\n"
                     "    BASEENGCRS[\"Engineering CRS\",\n"
                     "        EDATUM[\"Engineering datum\"]],\n"
                     "    DERIVINGCONVERSION[\"unnamed\",\n"
                     "        METHOD[\"PROJ unimplemented\"],\n"
                     "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_DerivedParametricCRS) {
    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "PARAMETRICCRS[\"Derived ParametricCRS\",\n"
        "    BASEPARAMCRS[\"Parametric CRS\",\n"
        "        PDATUM[\"Parametric datum\"]],\n"
        "    DERIVINGCONVERSION[\"unnamed\",\n"
        "        METHOD[\"PROJ unimplemented\"],\n"
        "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
        "    CS[parametric,1],\n"
        "        AXIS[\"pressure (hPa)\",up,\n"
        "            PARAMETRICUNIT[\"HectoPascal\",100]]]"));

    // Missing DERIVINGCONVERSION
    EXPECT_THROW(WKTParser().createFromWKT(
                     "PARAMETRICCRS[\"Derived ParametricCRS\",\n"
                     "    BASEPARAMCRS[\"Parametric CRS\",\n"
                     "        PDATUM[\"Parametric datum\"]],\n"
                     "    CS[parametric,1],\n"
                     "        AXIS[\"pressure (hPa)\",up,\n"
                     "            PARAMETRICUNIT[\"HectoPascal\",100]]]"),
                 ParsingException);

    // Missing CS
    EXPECT_THROW(WKTParser().createFromWKT(
                     "PARAMETRICCRS[\"Derived ParametricCRS\",\n"
                     "    BASEPARAMCRS[\"Parametric CRS\",\n"
                     "        PDATUM[\"Parametric datum\"]],\n"
                     "    DERIVINGCONVERSION[\"unnamed\",\n"
                     "        METHOD[\"PROJ unimplemented\"],\n"
                     "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]]]"),
                 ParsingException);

    // Wrong CS type
    EXPECT_THROW(WKTParser().createFromWKT(
                     "PARAMETRICCRS[\"Derived ParametricCRS\",\n"
                     "    BASEPARAMCRS[\"Parametric CRS\",\n"
                     "        PDATUM[\"Parametric datum\"]],\n"
                     "    DERIVINGCONVERSION[\"unnamed\",\n"
                     "        METHOD[\"PROJ unimplemented\"],\n"
                     "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
                     "    CS[TemporalDateTime,1],\n"
                     "        AXIS[\"time (T)\",future]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(wkt_parse, invalid_DerivedTemporalCRS) {
    EXPECT_NO_THROW(WKTParser().createFromWKT(
        "TIMECRS[\"Derived TemporalCRS\",\n"
        "    BASETIMECRS[\"Temporal CRS\",\n"
        "        TDATUM[\"Gregorian calendar\",\n"
        "            CALENDAR[\"proleptic Gregorian\"],\n"
        "            TIMEORIGIN[0000-01-01]]],\n"
        "    DERIVINGCONVERSION[\"unnamed\",\n"
        "        METHOD[\"PROJ unimplemented\"],\n"
        "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
        "    CS[TemporalDateTime,1],\n"
        "        AXIS[\"time (T)\",future]]"));

    // Missing DERIVINGCONVERSION
    EXPECT_THROW(WKTParser().createFromWKT(
                     "TIMECRS[\"Derived TemporalCRS\",\n"
                     "    BASETIMECRS[\"Temporal CRS\",\n"
                     "        TDATUM[\"Gregorian calendar\",\n"
                     "            CALENDAR[\"proleptic Gregorian\"],\n"
                     "            TIMEORIGIN[0000-01-01]]],\n"
                     "    CS[TemporalDateTime,1],\n"
                     "        AXIS[\"time (T)\",future]]"),
                 ParsingException);

    // Missing CS
    EXPECT_THROW(WKTParser().createFromWKT(
                     "TIMECRS[\"Derived TemporalCRS\",\n"
                     "    BASETIMECRS[\"Temporal CRS\",\n"
                     "        TDATUM[\"Gregorian calendar\",\n"
                     "            CALENDAR[\"proleptic Gregorian\"],\n"
                     "            TIMEORIGIN[0000-01-01]]],\n"
                     "    DERIVINGCONVERSION[\"unnamed\",\n"
                     "        METHOD[\"PROJ unimplemented\"],\n"
                     "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]]]"),
                 ParsingException);

    // Wrong CS type
    EXPECT_THROW(WKTParser().createFromWKT(
                     "TIMECRS[\"Derived TemporalCRS\",\n"
                     "    BASETIMECRS[\"Temporal CRS\",\n"
                     "        TDATUM[\"Gregorian calendar\",\n"
                     "            CALENDAR[\"proleptic Gregorian\"],\n"
                     "            TIMEORIGIN[0000-01-01]]],\n"
                     "    DERIVINGCONVERSION[\"unnamed\",\n"
                     "        METHOD[\"PROJ unimplemented\"],\n"
                     "        PARAMETER[\"foo\",1.0,UNIT[\"metre\",1]]],\n"
                     "    CS[parametric,1],\n"
                     "        AXIS[\"pressure (hPa)\",up,\n"
                     "            PARAMETRICUNIT[\"HectoPascal\",100]]]"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(io, projstringformatter) {

    {
        auto fmt = PROJStringFormatter::create();
        fmt->addStep("my_proj");
        EXPECT_EQ(fmt->toString(), "+proj=my_proj");
    }

    {
        auto fmt = PROJStringFormatter::create();
        fmt->addStep("my_proj");
        fmt->setCurrentStepInverted(true);
        EXPECT_EQ(fmt->toString(), "+proj=pipeline +step +inv +proj=my_proj");
    }

    {
        auto fmt = PROJStringFormatter::create();
        fmt->addStep("my_proj1");
        fmt->addStep("my_proj2");
        EXPECT_EQ(fmt->toString(),
                  "+proj=pipeline +step +proj=my_proj1 +step +proj=my_proj2");
    }

    {
        auto fmt = PROJStringFormatter::create();
        fmt->addStep("my_proj1");
        fmt->setCurrentStepInverted(true);
        fmt->addStep("my_proj2");
        EXPECT_EQ(
            fmt->toString(),
            "+proj=pipeline +step +inv +proj=my_proj1 +step +proj=my_proj2");
    }

    {
        auto fmt = PROJStringFormatter::create();
        fmt->startInversion();
        fmt->addStep("my_proj1");
        fmt->setCurrentStepInverted(true);
        fmt->addStep("my_proj2");
        fmt->stopInversion();
        EXPECT_EQ(
            fmt->toString(),
            "+proj=pipeline +step +inv +proj=my_proj2 +step +proj=my_proj1");
    }

    {
        auto fmt = PROJStringFormatter::create();
        fmt->startInversion();
        fmt->addStep("my_proj1");
        fmt->setCurrentStepInverted(true);
        fmt->startInversion();
        fmt->addStep("my_proj2");
        fmt->stopInversion();
        fmt->stopInversion();
        EXPECT_EQ(fmt->toString(),
                  "+proj=pipeline +step +proj=my_proj2 +step +proj=my_proj1");
    }
}

// ---------------------------------------------------------------------------

TEST(io, projstringformatter_helmert_3_param_noop) {
    auto fmt = PROJStringFormatter::create();
    fmt->addStep("helmert");
    fmt->addParam("x", 0);
    fmt->addParam("y", 0);
    fmt->addParam("z", 0);
    EXPECT_EQ(fmt->toString(), "");
}

// ---------------------------------------------------------------------------

TEST(io, projstringformatter_helmert_7_param_noop) {
    auto fmt = PROJStringFormatter::create();
    fmt->addStep("helmert");
    fmt->addParam("x", 0);
    fmt->addParam("y", 0);
    fmt->addParam("z", 0);
    fmt->addParam("rx", 0);
    fmt->addParam("ry", 0);
    fmt->addParam("rz", 0);
    fmt->addParam("s", 0);
    fmt->addParam("convention", "position_vector");
    EXPECT_EQ(fmt->toString(), "");
}

// ---------------------------------------------------------------------------

TEST(io, projstringformatter_merge_consecutive_helmert_3_param) {
    auto fmt = PROJStringFormatter::create();
    fmt->addStep("helmert");
    fmt->addParam("x", 10);
    fmt->addParam("y", 20);
    fmt->addParam("z", 30);
    fmt->addStep("helmert");
    fmt->addParam("x", -1);
    fmt->addParam("y", -2);
    fmt->addParam("z", -3);
    EXPECT_EQ(fmt->toString(), "+proj=helmert +x=9 +y=18 +z=27");
}

// ---------------------------------------------------------------------------

TEST(io, projstringformatter_merge_consecutive_helmert_3_param_noop) {
    auto fmt = PROJStringFormatter::create();
    fmt->addStep("helmert");
    fmt->addParam("x", 10);
    fmt->addParam("y", 20);
    fmt->addParam("z", 30);
    fmt->addStep("helmert");
    fmt->addParam("x", -10);
    fmt->addParam("y", -20);
    fmt->addParam("z", -30);
    EXPECT_EQ(fmt->toString(), "");
}

// ---------------------------------------------------------------------------

TEST(io, projstringformatter_cart_grs80_wgs84) {
    auto fmt = PROJStringFormatter::create();
    fmt->addStep("cart");
    fmt->addParam("ellps", "WGS84");
    fmt->addStep("cart");
    fmt->setCurrentStepInverted(true);
    fmt->addParam("ellps", "GRS80");
    EXPECT_EQ(fmt->toString(), "");
}

// ---------------------------------------------------------------------------

TEST(io, projstringformatter_axisswap_unitconvert_axisswap) {
    auto fmt = PROJStringFormatter::create();
    fmt->addStep("axisswap");
    fmt->addParam("order", "2,1");
    fmt->addStep("unitconvert");
    fmt->addParam("xy_in", "rad");
    fmt->addParam("xy_out", "deg");
    fmt->addStep("axisswap");
    fmt->addParam("order", "2,1");
    EXPECT_EQ(fmt->toString(), "+proj=unitconvert +xy_in=rad +xy_out=deg");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat) {

    auto expected = "GEODCRS[\"unknown\",\n"
                    "    DATUM[\"World Geodetic System 1984\",\n"
                    "        ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
                    "            LENGTHUNIT[\"metre\",1]],\n"
                    "        ID[\"EPSG\",6326]],\n"
                    "    PRIMEM[\"Greenwich\",0,\n"
                    "        ANGLEUNIT[\"degree\",0.0174532925199433],\n"
                    "        ID[\"EPSG\",8901]],\n"
                    "    CS[ellipsoidal,2],\n"
                    "        AXIS[\"longitude\",east,\n"
                    "            ORDER[1],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
                    "                ID[\"EPSG\",9122]]],\n"
                    "        AXIS[\"latitude\",north,\n"
                    "            ORDER[2],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
                    "                ID[\"EPSG\",9122]]]]";
    {
        auto obj = PROJStringParser().createFromPROJString("+proj=longlat");
        auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
        ASSERT_TRUE(crs != nullptr);
        WKTFormatterNNPtr f(WKTFormatter::create());
        crs->exportToWKT(f.get());
        EXPECT_EQ(f->toString(), expected);
    }

    {
        auto obj = PROJStringParser().createFromPROJString(
            "+proj=longlat +datum=WGS84");
        auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
        ASSERT_TRUE(crs != nullptr);
        WKTFormatterNNPtr f(WKTFormatter::create());
        crs->exportToWKT(f.get());
        EXPECT_EQ(f->toString(), expected);
    }
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_datum_NAD83) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +datum=NAD83");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    crs->exportToWKT(f.get());
    EXPECT_EQ(f->toString(),
              "GEODCRS[\"unknown\",\n"
              "    DATUM[\"North American Datum 1983\",\n"
              "        ELLIPSOID[\"GRS 1980\",6378137,298.257222101,\n"
              "            LENGTHUNIT[\"metre\",1]],\n"
              "        ID[\"EPSG\",6269]],\n"
              "    PRIMEM[\"Greenwich\",0,\n"
              "        ANGLEUNIT[\"degree\",0.0174532925199433],\n"
              "        ID[\"EPSG\",8901]],\n"
              "    CS[ellipsoidal,2],\n"
              "        AXIS[\"longitude\",east,\n"
              "            ORDER[1],\n"
              "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
              "                ID[\"EPSG\",9122]]],\n"
              "        AXIS[\"latitude\",north,\n"
              "            ORDER[2],\n"
              "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
              "                ID[\"EPSG\",9122]]]]");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_datum_NAD27) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +datum=NAD27");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    crs->exportToWKT(f.get());
    EXPECT_EQ(f->toString(),
              "GEODCRS[\"unknown\",\n"
              "    DATUM[\"North American Datum 1927\",\n"
              "        ELLIPSOID[\"Clarke 1866\",6378206.4,294.978698213898,\n"
              "            LENGTHUNIT[\"metre\",1]],\n"
              "        ID[\"EPSG\",6267]],\n"
              "    PRIMEM[\"Greenwich\",0,\n"
              "        ANGLEUNIT[\"degree\",0.0174532925199433],\n"
              "        ID[\"EPSG\",8901]],\n"
              "    CS[ellipsoidal,2],\n"
              "        AXIS[\"longitude\",east,\n"
              "            ORDER[1],\n"
              "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
              "                ID[\"EPSG\",9122]]],\n"
              "        AXIS[\"latitude\",north,\n"
              "            ORDER[2],\n"
              "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
              "                ID[\"EPSG\",9122]]]]");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_datum_other) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +datum=carthage");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    crs->exportToWKT(f.get());
    EXPECT_EQ(f->toString(),
              "GEODCRS[\"unknown\",\n"
              "    DATUM[\"Carthage\",\n"
              "        ELLIPSOID[\"Clarke 1880 (IGN)\",6378249.2,293.4660213,\n"
              "            LENGTHUNIT[\"metre\",1]],\n"
              "        ID[\"EPSG\",6223]],\n"
              "    PRIMEM[\"Greenwich\",0,\n"
              "        ANGLEUNIT[\"degree\",0.0174532925199433],\n"
              "        ID[\"EPSG\",8901]],\n"
              "    CS[ellipsoidal,2],\n"
              "        AXIS[\"longitude\",east,\n"
              "            ORDER[1],\n"
              "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
              "                ID[\"EPSG\",9122]]],\n"
              "        AXIS[\"latitude\",north,\n"
              "            ORDER[2],\n"
              "            ANGLEUNIT[\"degree\",0.0174532925199433,\n"
              "                ID[\"EPSG\",9122]]]]");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_ellps_WGS84) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +ellps=WGS84");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected = "GEODCRS[\"unknown\",\n"
                    "    DATUM[\"Unknown based on WGS84 ellipsoid\",\n"
                    "        ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
                    "            LENGTHUNIT[\"metre\",1]]],\n"
                    "    PRIMEM[\"Greenwich\",0,\n"
                    "        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "    CS[ellipsoidal,2],\n"
                    "        AXIS[\"longitude\",east,\n"
                    "            ORDER[1],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "        AXIS[\"latitude\",north,\n"
                    "            ORDER[2],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]]]";
    EXPECT_EQ(f->toString(), expected);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_ellps_GRS80) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +ellps=GRS80");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected = "GEODCRS[\"unknown\",\n"
                    "    DATUM[\"Unknown based on GRS80 ellipsoid\",\n"
                    "        ELLIPSOID[\"GRS 1980\",6378137,298.257222101,\n"
                    "            LENGTHUNIT[\"metre\",1]]],\n"
                    "    PRIMEM[\"Greenwich\",0,\n"
                    "        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "    CS[ellipsoidal,2],\n"
                    "        AXIS[\"longitude\",east,\n"
                    "            ORDER[1],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "        AXIS[\"latitude\",north,\n"
                    "            ORDER[2],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]]]";
    EXPECT_EQ(f->toString(), expected);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_a_b) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +a=2 +b=1.5");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected = "GEODCRS[\"unknown\",\n"
                    "    DATUM[\"unknown\",\n"
                    "        ELLIPSOID[\"unknown\",2,4,\n"
                    "            LENGTHUNIT[\"metre\",1]]],\n"
                    "    PRIMEM[\"Reference meridian\",0,\n"
                    "        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "    CS[ellipsoidal,2],\n"
                    "        AXIS[\"longitude\",east,\n"
                    "            ORDER[1],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "        AXIS[\"latitude\",north,\n"
                    "            ORDER[2],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]]]";
    EXPECT_EQ(f->toString(), expected);
    EXPECT_EQ(crs->ellipsoid()->celestialBody(), "Non-Earth body");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_a_rf_WGS84) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +a=6378137 +rf=298.257223563");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected = "GEODCRS[\"unknown\",\n"
                    "    DATUM[\"unknown\",\n"
                    "        ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
                    "            LENGTHUNIT[\"metre\",1]]],\n"
                    "    PRIMEM[\"Greenwich\",0,\n"
                    "        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "    CS[ellipsoidal,2],\n"
                    "        AXIS[\"longitude\",east,\n"
                    "            ORDER[1],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "        AXIS[\"latitude\",north,\n"
                    "            ORDER[2],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]]]";
    EXPECT_EQ(f->toString(), expected);
    EXPECT_EQ(crs->ellipsoid()->celestialBody(), Ellipsoid::EARTH);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_a_rf) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +a=2 +rf=4");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected = "GEODCRS[\"unknown\",\n"
                    "    DATUM[\"unknown\",\n"
                    "        ELLIPSOID[\"unknown\",2,4,\n"
                    "            LENGTHUNIT[\"metre\",1]]],\n"
                    "    PRIMEM[\"Reference meridian\",0,\n"
                    "        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "    CS[ellipsoidal,2],\n"
                    "        AXIS[\"longitude\",east,\n"
                    "            ORDER[1],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "        AXIS[\"latitude\",north,\n"
                    "            ORDER[2],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]]]";
    EXPECT_EQ(f->toString(), expected);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_a_f_zero) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +a=2 +f=0");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected = "GEODCRS[\"unknown\",\n"
                    "    DATUM[\"unknown\",\n"
                    "        ELLIPSOID[\"unknown\",2,0,\n"
                    "            LENGTHUNIT[\"metre\",1]]],\n"
                    "    PRIMEM[\"Reference meridian\",0,\n"
                    "        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "    CS[ellipsoidal,2],\n"
                    "        AXIS[\"longitude\",east,\n"
                    "            ORDER[1],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "        AXIS[\"latitude\",north,\n"
                    "            ORDER[2],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]]]";
    EXPECT_EQ(f->toString(), expected);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_a_f_non_zero) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +a=2 +f=0.5");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->ellipsoid()->semiMajorAxis().getSIValue(), 2);
    auto rf = crs->ellipsoid()->computedInverseFlattening();
    EXPECT_EQ(rf, 2) << rf;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_a_e) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +a=2 +e=0.5");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->ellipsoid()->semiMajorAxis().getSIValue(), 2);
    auto rf = crs->ellipsoid()->computedInverseFlattening();
    EXPECT_NEAR(rf, 7.46410161513775, 1e-14) << rf;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_a_es) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +a=2 +es=0.5");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->ellipsoid()->semiMajorAxis().getSIValue(), 2);
    auto rf = crs->ellipsoid()->computedInverseFlattening();
    EXPECT_EQ(rf, 3.4142135623730958) << rf;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_R) {
    auto obj = PROJStringParser().createFromPROJString("+proj=longlat +R=2");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_TRUE(crs->ellipsoid()->isSphere());
    EXPECT_EQ(crs->ellipsoid()->semiMajorAxis().getSIValue(), 2);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_a) {
    auto obj = PROJStringParser().createFromPROJString("+proj=longlat +a=2");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_TRUE(crs->ellipsoid()->isSphere());
    EXPECT_EQ(crs->ellipsoid()->semiMajorAxis().getSIValue(), 2);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_a_override_ellps) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +a=2 +ellps=WGS84");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_TRUE(!crs->ellipsoid()->isSphere());
    EXPECT_EQ(crs->ellipsoid()->semiMajorAxis().getSIValue(), 2);
    EXPECT_EQ(crs->ellipsoid()->computedInverseFlattening(), 298.25722356300003)
        << crs->ellipsoid()->computedInverseFlattening();
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_pm_paris) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=longlat +pm=paris");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected = "GEODCRS[\"unknown\",\n"
                    "    DATUM[\"Unknown based on WGS 84 ellipsoid\",\n"
                    "        ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
                    "            LENGTHUNIT[\"metre\",1]]],\n"
                    "    PRIMEM[\"Paris\",2.5969213,\n"
                    "        ANGLEUNIT[\"grad\",0.015707963267949]],\n"
                    "    CS[ellipsoidal,2],\n"
                    "        AXIS[\"longitude\",east,\n"
                    "            ORDER[1],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "        AXIS[\"latitude\",north,\n"
                    "            ORDER[2],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]]]";

    EXPECT_EQ(f->toString(), expected);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_pm_ferro) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +ellps=bessel +pm=ferro");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected =
        "GEODCRS[\"unknown\",\n"
        "    DATUM[\"Unknown based on Bessel 1841 ellipsoid\",\n"
        "        ELLIPSOID[\"Bessel 1841\",6377397.155,299.1528128,\n"
        "            LENGTHUNIT[\"metre\",1]]],\n"
        "    PRIMEM[\"Ferro\",-17.6666666666667,\n"
        "        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "    CS[ellipsoidal,2],\n"
        "        AXIS[\"longitude\",east,\n"
        "            ORDER[1],\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
        "        AXIS[\"latitude\",north,\n"
        "            ORDER[2],\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433]]]";

    EXPECT_EQ(f->toString(), expected);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_pm_numeric) {
    auto obj = PROJStringParser().createFromPROJString("+proj=longlat +pm=2.5");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected = "GEODCRS[\"unknown\",\n"
                    "    DATUM[\"Unknown based on WGS 84 ellipsoid\",\n"
                    "        ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
                    "            LENGTHUNIT[\"metre\",1]]],\n"
                    "    PRIMEM[\"unknown\",2.5,\n"
                    "        ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "    CS[ellipsoidal,2],\n"
                    "        AXIS[\"longitude\",east,\n"
                    "            ORDER[1],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]],\n"
                    "        AXIS[\"latitude\",north,\n"
                    "            ORDER[2],\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]]]";

    EXPECT_EQ(f->toString(), expected);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_pm_overriding_datum) {
    // It is arguable that we allow the prime meridian of a datum defined by
    // its name to be overridden, but this is found at least in a regression
    // test
    // of GDAL. So let's keep the ellipsoid part of the datum in that case and
    // use the specified prime meridian.
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +datum=WGS84 +pm=ferro");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->datum()->nameStr(), "Unknown based on WGS 84 ellipsoid");
    EXPECT_EQ(crs->datum()->primeMeridian()->nameStr(), "Ferro");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_complex) {
    std::string input =
        "+proj=pipeline +step +proj=longlat +ellps=clrk80ign "
        "+pm=paris +step +proj=unitconvert +xy_in=rad +xy_out=grad +step "
        "+proj=axisswap +order=2,1";
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_towgs84_3_terms) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +ellps=GRS80 +towgs84=1.2,2,3");
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());

    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("METHOD[\"Geocentric translations") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"X-axis translation\",1.2") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Y-axis translation\",2") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Z-axis translation\",3") !=
                std::string::npos)
        << wkt;

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=longlat +ellps=GRS80 +towgs84=1.2,2,3,0,0,0,0 +no_defs");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_towgs84_7_terms) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +ellps=GRS80 +towgs84=1.2,2,3,4,5,6,7");
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());

    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("METHOD[\"Position Vector transformation") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"X-axis translation\",1.2") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Y-axis translation\",2") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Z-axis translation\",3") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Scale difference\",1.000007") !=
                std::string::npos)
        << wkt;

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=longlat +ellps=GRS80 +towgs84=1.2,2,3,4,5,6,7 +no_defs");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_nadgrids) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +ellps=GRS80 +nadgrids=foo.gsb");
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());

    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("METHOD[\"NTv2\"") != std::string::npos) << wkt;
    EXPECT_TRUE(wkt.find("PARAMETERFILE[\"Latitude and longitude difference "
                         "file\",\"foo.gsb\"]") != std::string::npos)
        << wkt;

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=longlat +ellps=GRS80 +nadgrids=foo.gsb +no_defs");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_geoidgrids) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +ellps=GRS80 +geoidgrids=foo.gtx");
    auto crs = nn_dynamic_pointer_cast<CompoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());

    auto wkt = f->toString();
    EXPECT_TRUE(
        wkt.find(
            "ABRIDGEDTRANSFORMATION[\"unknown to WGS84 ellipsoidal height\"") !=
        std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETERFILE[\"Geoid (height correction) model "
                         "file\",\"foo.gtx\"]") != std::string::npos)
        << wkt;

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=longlat +ellps=GRS80 +geoidgrids=foo.gtx +vunits=m +no_defs");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_geoidgrids_vunits) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +ellps=GRS80 +geoidgrids=foo.gtx +vunits=ft");
    auto crs = nn_dynamic_pointer_cast<CompoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());

    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("AXIS[\"gravity-related height "
                         "(H)\",up,LENGTHUNIT[\"foot\",0.3048]") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_vunits) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +ellps=GRS80 +vunits=ft");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());

    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("AXIS[\"ellipsoidal height "
                         "(h)\",up,ORDER[3],LENGTHUNIT[\"foot\",0.3048]") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_vunits) {
    auto obj = PROJStringParser().createFromPROJString("+vunits=ft");
    auto crs = nn_dynamic_pointer_cast<VerticalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
              "+vunits=ft");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_vto_meter) {
    auto obj = PROJStringParser().createFromPROJString("+vto_meter=2");
    auto crs = nn_dynamic_pointer_cast<VerticalCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
              "+vto_meter=2");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_axis_enu) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +ellps=GRS80 +axis=enu");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());

    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("AXIS[\"longitude\",east,ORDER[1]") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("AXIS[\"latitude\",north,ORDER[2]") !=
                std::string::npos)
        << wkt;

    EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
              "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
              "+proj=unitconvert +xy_in=rad +xy_out=deg");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_axis_neu) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +ellps=GRS80 +axis=neu");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());

    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("AXIS[\"latitude\",north,ORDER[1]") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("AXIS[\"longitude\",east,ORDER[2]") !=
                std::string::npos)
        << wkt;

    EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
              "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
              "+proj=unitconvert +xy_in=rad +xy_out=deg +step +proj=axisswap "
              "+order=2,1");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_axis_swu) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=longlat +ellps=GRS80 +axis=swu");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());

    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("AXIS[\"latitude\",south,ORDER[1]") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("AXIS[\"longitude\",west,ORDER[2]") !=
                std::string::npos)
        << wkt;

    EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
              "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
              "+proj=unitconvert +xy_in=rad +xy_out=deg +step +proj=axisswap "
              "+order=-2,-1");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_unitconvert_deg) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
        "+proj=unitconvert +xy_in=rad +xy_out=deg");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
              "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
              "+proj=unitconvert +xy_in=rad +xy_out=deg");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_unitconvert_grad) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
        "+proj=unitconvert +xy_in=rad +xy_out=grad");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
              "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
              "+proj=unitconvert +xy_in=rad +xy_out=grad");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_unitconvert_rad) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
        "+proj=unitconvert +xy_in=rad +xy_out=rad");
    auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
              "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
              "+proj=unitconvert +xy_in=rad +xy_out=rad");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_axisswap) {
    for (auto order1 : {"1", "-1", "2", "-2"}) {
        for (auto order2 : {"1", "-1", "2", "-2"}) {
            if (std::abs(atoi(order1) * atoi(order2)) == 2 &&
                !(atoi(order1) == 1 && atoi(order2) == 2)) {
                auto str = "+proj=pipeline +step +proj=longlat +ellps=GRS80 "
                           "+step +proj=axisswap +order=" +
                           std::string(order1) + "," + order2;
                auto obj = PROJStringParser().createFromPROJString(str);
                auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
                ASSERT_TRUE(crs != nullptr);

                EXPECT_EQ(
                    crs->exportToPROJString(
                        PROJStringFormatter::create().get()),
                    "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
                    "+proj=unitconvert +xy_in=rad +xy_out=deg +step "
                    "+proj=axisswap +order=" +
                        std::string(order1) + "," + order2);
            }
        }
    }
}

// ---------------------------------------------------------------------------

TEST(io, projparse_tmerc) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=tmerc +x_0=1 +lat_0=1 +k_0=2");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected = "PROJCRS[\"unknown\",\n"
                    "    BASEGEODCRS[\"unknown\",\n"
                    "        DATUM[\"World Geodetic System 1984\",\n"
                    "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
                    "                LENGTHUNIT[\"metre\",1]]],\n"
                    "        PRIMEM[\"Greenwich\",0,\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
                    "    CONVERSION[\"unknown\",\n"
                    "        METHOD[\"Transverse Mercator\",\n"
                    "            ID[\"EPSG\",9807]],\n"
                    "        PARAMETER[\"Latitude of natural origin\",1,\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
                    "            ID[\"EPSG\",8801]],\n"
                    "        PARAMETER[\"Longitude of natural origin\",0,\n"
                    "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
                    "            ID[\"EPSG\",8802]],\n"
                    "        PARAMETER[\"Scale factor at natural origin\",2,\n"
                    "            SCALEUNIT[\"unity\",1],\n"
                    "            ID[\"EPSG\",8805]],\n"
                    "        PARAMETER[\"False easting\",1,\n"
                    "            LENGTHUNIT[\"metre\",1],\n"
                    "            ID[\"EPSG\",8806]],\n"
                    "        PARAMETER[\"False northing\",0,\n"
                    "            LENGTHUNIT[\"metre\",1],\n"
                    "            ID[\"EPSG\",8807]]],\n"
                    "    CS[Cartesian,2],\n"
                    "        AXIS[\"(E)\",east,\n"
                    "            ORDER[1],\n"
                    "            LENGTHUNIT[\"metre\",1]],\n"
                    "        AXIS[\"(N)\",north,\n"
                    "            ORDER[2],\n"
                    "            LENGTHUNIT[\"metre\",1]]]";

    EXPECT_EQ(f->toString(), expected);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_tmerc_south_oriented) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=tmerc +axis=wsu +x_0=1 +lat_0=1 +k_0=2");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    crs->exportToWKT(f.get());
    auto expected =
        "PROJCRS[\"unknown\",\n"
        "    BASEGEODCRS[\"unknown\",\n"
        "        DATUM[\"World Geodetic System 1984\",\n"
        "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
        "                LENGTHUNIT[\"metre\",1]]],\n"
        "        PRIMEM[\"Greenwich\",0,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433]]],\n"
        "    CONVERSION[\"unknown\",\n"
        "        METHOD[\"Transverse Mercator (South Orientated)\",\n"
        "            ID[\"EPSG\",9808]],\n"
        "        PARAMETER[\"Latitude of natural origin\",1,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8801]],\n"
        "        PARAMETER[\"Longitude of natural origin\",0,\n"
        "            ANGLEUNIT[\"degree\",0.0174532925199433],\n"
        "            ID[\"EPSG\",8802]],\n"
        "        PARAMETER[\"Scale factor at natural origin\",2,\n"
        "            SCALEUNIT[\"unity\",1],\n"
        "            ID[\"EPSG\",8805]],\n"
        "        PARAMETER[\"False easting\",1,\n"
        "            LENGTHUNIT[\"metre\",1],\n"
        "            ID[\"EPSG\",8806]],\n"
        "        PARAMETER[\"False northing\",0,\n"
        "            LENGTHUNIT[\"metre\",1],\n"
        "            ID[\"EPSG\",8807]]],\n"
        "    CS[Cartesian,2],\n"
        "        AXIS[\"westing\",west,\n"
        "            ORDER[1],\n"
        "            LENGTHUNIT[\"metre\",1]],\n"
        "        AXIS[\"southing\",south,\n"
        "            ORDER[2],\n"
        "            LENGTHUNIT[\"metre\",1]]]";

    EXPECT_EQ(f->toString(), expected);

    obj = PROJStringParser().createFromPROJString(
        "+proj=pipeline +step +proj=tmerc +x_0=1 +lat_0=1 +k_0=2 +step "
        "+proj=axisswap +order=-1,-2");
    crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    EXPECT_EQ(crs->derivingConversion()->method()->nameStr(),
              "Transverse Mercator (South Orientated)");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_lcc_as_lcc1sp) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=lcc +lat_0=45 +lat_1=45");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("Lambert Conic Conformal (1SP)") != std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_lcc_as_lcc2sp) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=lcc +lat_0=45 +lat_1=46");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("Lambert Conic Conformal (2SP)") != std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_lcc_as_lcc2sp_michigan) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=lcc +lat_0=45 +lat_1=46 +k_0=1.02");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("Lambert Conic Conformal (2SP Michigan)") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_aeqd_guam) {
    auto obj = PROJStringParser().createFromPROJString("+proj=aeqd +guam");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("Guam Projection") != std::string::npos) << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_cea_ellipsoidal) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=cea +ellps=GRS80");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(
        wkt.find(
            "METHOD[\"Lambert Cylindrical Equal Area\",ID[\"EPSG\",9835]]") !=
        std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_geos_sweep_x) {
    auto obj = PROJStringParser().createFromPROJString("+proj=geos +sweep=x");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("Geostationary Satellite (Sweep X)") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_geos_sweep_y) {
    auto obj = PROJStringParser().createFromPROJString("+proj=geos");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("Geostationary Satellite (Sweep Y)") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_omerc_nouoff) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=omerc +no_uoff +alpha=2 +gamma=3");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("METHOD[\"Hotine Oblique Mercator (variant "
                         "A)\",ID[\"EPSG\",9812]]") != std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Azimuth of initial line\",2") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Angle from Rectified to Skew Grid\",3") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_omerc_tpno) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=omerc +lat_1=1 +lat_2=2 +lon_1=3 +lon_2=4");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(
        wkt.find(
            "METHOD[\"Hotine Oblique Mercator Two Point Natural Origin\"]") !=
        std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_omerc_variant_b) {
    auto obj = PROJStringParser().createFromPROJString("+proj=omerc +alpha=2");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("METHOD[\"Hotine Oblique Mercator (variant "
                         "B)\",ID[\"EPSG\",9815]]") != std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Angle from Rectified to Skew Grid\",2") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_somerc) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=somerc +lat_0=1 +lon_0=2 +k_0=3 +x_0=4 +y_0=5");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("METHOD[\"Hotine Oblique Mercator (variant "
                         "B)\",ID[\"EPSG\",9815]]") != std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"Latitude of projection centre\",1") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"Longitude of projection centre\",2") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"Scale factor on initial line\",3") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"Azimuth of initial line\",90") != std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"Angle from Rectified to Skew Grid\",90") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"Easting at projection centre\",4") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"Northing at projection centre\",5") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_krovak) {
    auto obj = PROJStringParser().createFromPROJString("+proj=krovak");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(
        wkt.find("METHOD[\"Krovak (North Orientated)\",ID[\"EPSG\",1041]]") !=
        std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_krovak_axis_swu) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=krovak +axis=swu");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("METHOD[\"Krovak\",ID[\"EPSG\",9819]]") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_etmerc) {
    auto obj = PROJStringParser().createFromPROJString("+proj=etmerc");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    auto wkt2 = crs->exportToWKT(
        &WKTFormatter::create()->simulCurNodeHasId().setMultiLine(false));
    EXPECT_TRUE(
        wkt2.find("METHOD[\"Transverse Mercator\",ID[\"EPSG\",9807]]") !=
        std::string::npos)
        << wkt2;

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=etmerc +lat_0=0 +lon_0=0 +k=1 +x_0=0 +y_0=0 "
        "+datum=WGS84 +units=m +no_defs");

    auto wkt1 = crs->exportToWKT(
        WKTFormatter::create(WKTFormatter::Convention::WKT1_GDAL).get());
    EXPECT_TRUE(wkt1.find("EXTENSION[\"PROJ4\",\"+proj=etmerc +lat_0=0 "
                          "+lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m "
                          "+no_defs\"]") != std::string::npos)
        << wkt1;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_merc_variant_B) {
    auto obj = PROJStringParser().createFromPROJString("+proj=merc +lat_ts=1");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(
        wkt.find("METHOD[\"Mercator (variant B)\",ID[\"EPSG\",9805]]") !=
        std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Latitude of 1st standard parallel\",1") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_merc_google_mercator) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 "
        "+k=1 +units=m +nadgrids=@null");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("METHOD[\"Popular Visualisation Pseudo "
                         "Mercator\",ID[\"EPSG\",1024]") != std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_merc_stere_polar_variant_B) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=stere +lat_0=90 +lat_ts=70");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(
        wkt.find(
            "METHOD[\"Polar Stereographic (variant B)\",ID[\"EPSG\",9829]]") !=
        std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_merc_stere_polar_variant_A) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=stere +lat_0=-90 +k=0.994");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(
        wkt.find(
            "METHOD[\"Polar Stereographic (variant A)\",ID[\"EPSG\",9810]]") !=
        std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_merc_stere_polar_k_and_lat_ts) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=stere +lat_0=90 +lat_ts=90 +k=1");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    auto wkt = crs->exportToWKT(
        &(WKTFormatter::create()->simulCurNodeHasId().setMultiLine(false)));
    EXPECT_TRUE(
        wkt.find(
            "METHOD[\"Polar Stereographic (variant B)\",ID[\"EPSG\",9829]]") !=
        std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Latitude of standard parallel\",90") !=
                std::string::npos)
        << wkt;
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=stere +lat_0=90 +lat_ts=90 +lon_0=0 +x_0=0 +y_0=0 +datum=WGS84 "
        "+units=m +no_defs");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_merc_stere_polar_k_and_lat_ts_incompatible) {
    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=stere +lat_0=90 +lat_ts=70 +k=0.994"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_merc_stere) {
    auto obj = PROJStringParser().createFromPROJString("+proj=stere +lat_0=30");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("METHOD[\"Stereographic\"]") != std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_utm) {
    auto obj = PROJStringParser().createFromPROJString("+proj=utm +zone=1");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("CONVERSION[\"UTM zone 1N\",METHOD[\"Transverse "
                         "Mercator\",ID[\"EPSG\",9807]]") != std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"Longitude of natural origin\",-177,") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"False northing\",0,") != std::string::npos) << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_utm_south) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=utm +zone=1 +south");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("CONVERSION[\"UTM zone 1S\",METHOD[\"Transverse "
                         "Mercator\",ID[\"EPSG\",9807]]") != std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"Longitude of natural origin\",-177,") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("\"False northing\",10000000,") != std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_laea_north_pole) {
    auto obj = PROJStringParser().createFromPROJString("+proj=laea +lat_0=90");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("AXIS[\"(E)\",south") != std::string::npos) << wkt;
    EXPECT_TRUE(wkt.find("AXIS[\"(N)\",south") != std::string::npos) << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_laea_south_pole) {
    auto obj = PROJStringParser().createFromPROJString("+proj=laea +lat_0=-90");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("AXIS[\"(E)\",north") != std::string::npos) << wkt;
    EXPECT_TRUE(wkt.find("AXIS[\"(N)\",north") != std::string::npos) << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_laea_spherical) {
    auto obj = PROJStringParser().createFromPROJString("+proj=laea +R=6371228");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->derivingConversion()->method()->getEPSGCode(),
              EPSG_CODE_METHOD_LAMBERT_AZIMUTHAL_EQUAL_AREA_SPHERICAL);

    auto crs2 = ProjectedCRS::create(
        PropertyMap(), crs->baseCRS(),
        Conversion::createLambertAzimuthalEqualArea(
            PropertyMap(), Angle(0), Angle(0), Length(0), Length(0)),
        crs->coordinateSystem());
    EXPECT_EQ(crs2->derivingConversion()->method()->getEPSGCode(),
              EPSG_CODE_METHOD_LAMBERT_AZIMUTHAL_EQUAL_AREA);

    EXPECT_TRUE(
        crs->isEquivalentTo(crs2.get(), IComparable::Criterion::EQUIVALENT));
    EXPECT_TRUE(
        crs2->isEquivalentTo(crs.get(), IComparable::Criterion::EQUIVALENT));
}

// ---------------------------------------------------------------------------

TEST(io, projparse_eqc_spherical) {
    auto obj = PROJStringParser().createFromPROJString("+proj=eqc +R=6371228");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(crs->derivingConversion()->method()->getEPSGCode(),
              EPSG_CODE_METHOD_EQUIDISTANT_CYLINDRICAL_SPHERICAL);

    auto crs2 = ProjectedCRS::create(
        PropertyMap(), crs->baseCRS(),
        Conversion::createEquidistantCylindrical(
            PropertyMap(), Angle(0), Angle(0), Length(0), Length(0)),
        crs->coordinateSystem());
    EXPECT_EQ(crs2->derivingConversion()->method()->getEPSGCode(),
              EPSG_CODE_METHOD_EQUIDISTANT_CYLINDRICAL);

    EXPECT_TRUE(
        crs->isEquivalentTo(crs2.get(), IComparable::Criterion::EQUIVALENT));
    EXPECT_TRUE(
        crs2->isEquivalentTo(crs.get(), IComparable::Criterion::EQUIVALENT));
}

// ---------------------------------------------------------------------------

TEST(io, projparse_non_earth_ellipsoid) {
    std::string input("+proj=merc +lon_0=0 +k=1 +x_0=0 +y_0=0 +R=1 +units=m "
                      "+no_defs");
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_axisswap_unitconvert_longlat_proj) {
    std::string input =
        "+proj=pipeline +step +proj=axisswap +order=2,1 +step "
        "+proj=unitconvert +xy_in=grad +xy_out=rad +step +inv +proj=longlat "
        "+ellps=clrk80ign +pm=paris +step +proj=lcc +lat_1=49.5 "
        "+lat_0=49.5 +lon_0=0 +k_0=0.999877341 +x_0=600000 +y_0=200000 "
        "+ellps=clrk80ign +pm=paris";
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_axisswap_unitconvert_proj_axisswap) {
    std::string input =
        "+proj=pipeline +step +proj=axisswap +order=2,1 +step "
        "+proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=igh "
        "+lon_0=0 +x_0=0 +y_0=0 +ellps=GRS80 +step +proj=axisswap +order=2,1";
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_axisswap_unitconvert_proj_unitconvert) {
    std::string input =
        "+proj=pipeline +step +proj=axisswap +order=2,1 +step "
        "+proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=igh "
        "+lon_0=0 +x_0=0 +y_0=0 +ellps=GRS80 +step +proj=unitconvert +xy_in=m "
        "+z_in=m +xy_out=ft +z_out=ft";
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_axisswap_unitconvert_proj_unitconvert_numeric_axisswap) {
    std::string input =
        "+proj=pipeline +step +proj=axisswap +order=2,1 +step "
        "+proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=igh "
        "+lon_0=0 +x_0=0 +y_0=0 +ellps=GRS80 +step +proj=unitconvert +xy_in=m "
        "+z_in=m +xy_out=2.5 +z_out=2.5 +step +proj=axisswap +order=-2,-1";
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_projected_units) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=tmerc +x_0=0.304800609601219 +units=us-ft");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("PARAMETER[\"False easting\",1,LENGTHUNIT[\"US survey "
                         "foot\",0.304800609601219]") != std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("AXIS[\"(E)\",east,ORDER[1],LENGTHUNIT[\"US survey "
                         "foot\",0.304800609601219]") != std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_projected_to_meter_known) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=tmerc +to_meter=0.304800609601219");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("PARAMETER[\"False easting\",0,LENGTHUNIT[\"US survey "
                         "foot\",0.304800609601219]") != std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("AXIS[\"(E)\",east,ORDER[1],LENGTHUNIT[\"US survey "
                         "foot\",0.304800609601219]") != std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_projected_to_meter_unknown) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=tmerc +to_meter=0.1234");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(
        wkt.find(
            "PARAMETER[\"False easting\",0,LENGTHUNIT[\"unknown\",0.1234]") !=
        std::string::npos)
        << wkt;
    EXPECT_TRUE(
        wkt.find("AXIS[\"(E)\",east,ORDER[1],LENGTHUNIT[\"unknown\",0.1234]") !=
        std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_projected_vunits) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=tmerc +vunits=ft");
    auto crs = nn_dynamic_pointer_cast<CompoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("CS[Cartesian,2]") != std::string::npos) << wkt;
    EXPECT_TRUE(wkt.find("CS[vertical,1],AXIS[\"gravity-related height "
                         "(H)\",up,LENGTHUNIT[\"foot\",0.3048]") !=
                std::string::npos)
        << wkt;
}

// ---------------------------------------------------------------------------

TEST(io, projparse_projected_unknown) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=mbt_s +unused_flag +lat_0=45 +lon_0=0 +k=1 +x_0=10 +y_0=0");
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);

    {
        WKTFormatterNNPtr f(WKTFormatter::create());
        f->simulCurNodeHasId();
        f->setMultiLine(false);
        crs->exportToWKT(f.get());
        auto wkt = f->toString();
        EXPECT_TRUE(
            wkt.find("CONVERSION[\"unknown\",METHOD[\"PROJ mbt_s "
                     "unused_flag\"],PARAMETER[\"lat_0\",45,ANGLEUNIT["
                     "\"degree\",0.0174532925199433]],PARAMETER[\"lon_0\","
                     "0,ANGLEUNIT[\"degree\",0.0174532925199433]],"
                     "PARAMETER[\"k\",1,SCALEUNIT[\"unity\",1]],PARAMETER["
                     "\"x_0\",10,LENGTHUNIT[\"metre\",1]],PARAMETER[\"y_0\","
                     "0,LENGTHUNIT[\"metre\",1]]]") != std::string::npos)
            << wkt;
    }

    std::string expected_wkt1 =
        "PROJCS[\"unknown\",GEOGCS[\"unknown\",DATUM[\"WGS_1984\",SPHEROID["
        "\"WGS "
        "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
        "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\","
        "\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\","
        "\"9122\"]]],"
        "PROJECTION[\"custom_proj4\"],UNIT[\"metre\",1,AUTHORITY[\"EPSG\","
        "\"9001\"]],AXIS[\"Easting\",EAST],AXIS[\"Northing\",NORTH],EXTENSION["
        "\"PROJ4\",\"+proj=mbt_s +datum=WGS84 +unused_flag +lat_0=45 "
        "+lon_0=0 +k=1 +x_0=10 +y_0=0 +wktext +no_defs\"]]";

    {
        WKTFormatterNNPtr f(
            WKTFormatter::create(WKTFormatter::Convention::WKT1_GDAL));
        f->simulCurNodeHasId();
        f->setMultiLine(false);
        crs->exportToWKT(f.get());
        auto wkt = f->toString();
        EXPECT_EQ(wkt, expected_wkt1);
    }

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=mbt_s +unused_flag +lat_0=45 +lon_0=0 +k=1 +x_0=10 "
        "+y_0=0 +datum=WGS84 +units=m +no_defs");

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
        "+step +proj=mbt_s +unused_flag +lat_0=45 +lon_0=0 +k=1 "
        "+x_0=10 +y_0=0 +ellps=WGS84");

    {
        auto obj2 = WKTParser().createFromWKT(expected_wkt1);
        auto crs2 = nn_dynamic_pointer_cast<ProjectedCRS>(obj2);
        ASSERT_TRUE(crs2 != nullptr);

        WKTFormatterNNPtr f(WKTFormatter::create());
        f->simulCurNodeHasId();
        f->setMultiLine(false);
        crs2->exportToWKT(f.get());
        auto wkt = f->toString();
        EXPECT_TRUE(
            wkt.find("CONVERSION[\"unknown\",METHOD[\"PROJ mbt_s "
                     "unused_flag\"],PARAMETER[\"lat_0\",45,ANGLEUNIT["
                     "\"degree\",0.0174532925199433]],PARAMETER[\"lon_0\","
                     "0,ANGLEUNIT[\"degree\",0.0174532925199433]],"
                     "PARAMETER[\"k\",1,SCALEUNIT[\"unity\",1]],PARAMETER["
                     "\"x_0\",10,LENGTHUNIT[\"metre\",1]],PARAMETER[\"y_0\","
                     "0,LENGTHUNIT[\"metre\",1]]]") != std::string::npos)
            << wkt;
    }
}

// ---------------------------------------------------------------------------

TEST(io, projparse_geocent) {
    auto obj =
        PROJStringParser().createFromPROJString("+proj=geocent +ellps=WGS84");
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_EQ(wkt, "GEODCRS[\"unknown\",DATUM[\"Unknown based on WGS84 "
                   "ellipsoid\",ELLIPSOID[\"WGS "
                   "84\",6378137,298.257223563,LENGTHUNIT[\"metre\",1]]],"
                   "PRIMEM[\"Greenwich\",0,ANGLEUNIT[\"degree\",0."
                   "0174532925199433]],CS[Cartesian,3],AXIS[\"(X)\","
                   "geocentricX,ORDER[1],LENGTHUNIT[\"metre\",1]],AXIS[\"(Y)\","
                   "geocentricY,ORDER[2],LENGTHUNIT[\"metre\",1]],AXIS[\"(Z)\","
                   "geocentricZ,ORDER[3],LENGTHUNIT[\"metre\",1]]]");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_geocent_towgs84) {
    auto obj = PROJStringParser().createFromPROJString(
        "+proj=geocent +ellps=WGS84 +towgs84=1,2,3");
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    crs->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(
        wkt.find("METHOD[\"Geocentric translations (geocentric domain)") !=
        std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"X-axis translation\",1") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Y-axis translation\",2") !=
                std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("PARAMETER[\"Z-axis translation\",3") !=
                std::string::npos)
        << wkt;

    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=geocent +ellps=WGS84 +towgs84=1,2,3,0,0,0,0 +units=m +no_defs");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_cart_unit) {
    std::string input(
        "+proj=pipeline +step +proj=cart +ellps=WGS84 +step "
        "+proj=unitconvert +xy_in=m +z_in=m +xy_out=km +z_out=km");
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_cart_unit_numeric) {
    std::string input(
        "+proj=pipeline +step +proj=cart +ellps=WGS84 +step "
        "+proj=unitconvert +xy_in=m +z_in=m +xy_out=500 +z_out=500");
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_wktext) {
    std::string input("+proj=longlat +foo +wktext");
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_geocent_wktext) {
    std::string input("+proj=geocent +foo +wktext");
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<GeodeticCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_projected_wktext) {
    std::string input("+proj=merc +foo +wktext");
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_ob_tran_longlat) {
    std::string input(
        "+proj=pipeline +step +proj=axisswap +order=2,1 +step "
        "+proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=ob_tran "
        "+o_proj=longlat +o_lat_p=52 +o_lon_p=-30 +lon_0=-25 +ellps=WGS84 "
        "+step +proj=unitconvert +xy_in=rad +xy_out=deg +step "
        "+proj=axisswap +order=2,1");
    auto obj = PROJStringParser().createFromPROJString(input);
    auto crs = nn_dynamic_pointer_cast<DerivedGeographicCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        input);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_helmert_translation) {
    std::string projString("+proj=helmert +x=1 +y=2 +z=3");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        projString);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_helmert_translation_inv) {
    std::string projString(
        "+proj=pipeline +step +inv +proj=helmert +x=1 +y=2 +z=3");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        "+proj=helmert +x=-1 +y=-2 +z=-3");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_helmert_position_vector) {
    std::string projString("+proj=helmert +x=1 +y=2 +z=3 +rx=4 +ry=5 +rz=6 "
                           "+s=7 +convention=position_vector");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        projString);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_helmert_position_vector_inv) {
    std::string projString("+proj=pipeline +step +inv +proj=helmert +x=1 +y=2 "
                           "+z=3 +rx=4 +ry=5 +rz=6 "
                           "+s=7 +convention=position_vector");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<CoordinateOperation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        projString);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_helmert_time_dependent_position_vector) {
    std::string projString("+proj=helmert +x=1 +y=2 +z=3 +rx=4 +ry=5 +rz=6 "
                           "+s=7 +dx=0.1 +dy=0.2 +dz=0.3 +drx=0.4 +dry=0.5 "
                           "+drz=0.6 +ds=0.7 +t_epoch=2018.5 "
                           "+convention=position_vector");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        projString);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_helmert_coordinate_frame) {
    std::string projString("+proj=helmert +x=1 +y=2 +z=3 +rx=4 +ry=5 +rz=6 "
                           "+s=7 +convention=coordinate_frame");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        projString);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_helmert_time_dependent_coordinate_frame) {
    std::string projString("+proj=helmert +x=1 +y=2 +z=3 +rx=4 +ry=5 +rz=6 "
                           "+s=7 +dx=0.1 +dy=0.2 +dz=0.3 +drx=0.4 +dry=0.5 "
                           "+drz=0.6 +ds=0.7 +t_epoch=2018.5 "
                           "+convention=coordinate_frame");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        projString);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_helmert_complex_pipeline) {
    std::string projString(
        "+proj=pipeline +step +proj=cart "
        "+ellps=WGS84 +step +proj=helmert +x=-1 +y=-2 +z=-3 +rx=-4 "
        "+ry=-5 +rz=-6 +s=-7 +convention=position_vector +step +inv "
        "+proj=cart +ellps=clrk80ign");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
        "+step +proj=cart +ellps=WGS84 +step +proj=helmert +x=-1 +y=-2 "
        "+z=-3 +rx=-4 +ry=-5 +rz=-6 +s=-7 +convention=position_vector "
        "+step +inv +proj=cart +ellps=clrk80ign +step +proj=unitconvert "
        "+xy_in=rad +xy_out=deg");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_helmert_complex_pipeline_2) {
    std::string projString(
        "+proj=pipeline +step +proj=axisswap +order=2,1 +step "
        "+proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=cart "
        "+ellps=WGS84 +step +proj=helmert +x=-1 +y=-2 +z=-3 +rx=-4 "
        "+ry=-5 +rz=-6 +s=-7 +convention=position_vector +step +inv "
        "+proj=cart +ellps=clrk80ign +step "
        "+proj=longlat +ellps=clrk80ign +pm=paris +step "
        "+proj=unitconvert +xy_in=rad +xy_out=grad +step +proj=axisswap "
        "+order=2,1");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        projString);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_helmert_errors) {
    // Missing convention
    EXPECT_THROW(PROJStringParser().createFromPROJString("+proj=helmert +rx=4"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=helmert +convention=unhandled"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=helmert +unhandled_keyword"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_molodensky) {
    std::string projString("+proj=molodensky +ellps=WGS84 +dx=84.87 +dy=96.49 "
                           "+dz=116.95 +da=251 +df=1.41927e-05");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);

    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    transf->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_EQ(
        wkt,
        "COORDINATEOPERATION[\"unknown\",SOURCECRS[GEODCRS[\"unknown\",DATUM["
        "\"Unknown based on WGS84 ellipsoid\",ELLIPSOID[\"WGS "
        "84\",6378137,298.257223563,LENGTHUNIT[\"metre\",1]]],PRIMEM["
        "\"Greenwich\",0,ANGLEUNIT[\"degree\",0.0174532925199433]],CS["
        "ellipsoidal,2],AXIS[\"longitude\",east,ORDER[1],ANGLEUNIT[\"degree\","
        "0.0174532925199433]],AXIS[\"latitude\",north,ORDER[2],ANGLEUNIT["
        "\"degree\",0.0174532925199433]]]],TARGETCRS[GEODCRS[\"unknown\",DATUM["
        "\"unknown\",ELLIPSOID[\"unknown\",6378388,297.000000198989,LENGTHUNIT["
        "\"metre\",1]]],PRIMEM[\"Greenwich\",0,ANGLEUNIT[\"degree\",0."
        "0174532925199433]],CS[ellipsoidal,2],AXIS[\"longitude\",east,ORDER[1],"
        "ANGLEUNIT[\"degree\",0.0174532925199433]],AXIS[\"latitude\",north,"
        "ORDER[2],ANGLEUNIT[\"degree\",0.0174532925199433]]]],METHOD["
        "\"Molodensky\",ID[\"EPSG\",9604]],PARAMETER[\"X-axis "
        "translation\",84.87,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8605]],"
        "PARAMETER[\"Y-axis "
        "translation\",96.49,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8606]],"
        "PARAMETER[\"Z-axis "
        "translation\",116.95,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8607]],"
        "PARAMETER[\"Semi-major axis length "
        "difference\",251,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8654]],PARAMETER["
        "\"Flattening difference\",1.41927e-05,ID[\"EPSG\",8655]]]");

    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
        "+step +proj=molodensky +ellps=WGS84 +dx=84.87 +dy=96.49 "
        "+dz=116.95 +da=251 +df=1.41927e-05 +step +proj=longlat "
        "+a=6378388 +rf=297.000000198989 +step +proj=unitconvert "
        "+xy_in=rad +xy_out=deg");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_molodensky_inv) {
    std::string projString("+proj=pipeline +step +inv +proj=molodensky "
                           "+ellps=WGS84 +dx=84.87 +dy=96.49 "
                           "+dz=116.95 +da=251 +df=1.41927e-05");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<CoordinateOperation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
        "+step +inv +proj=longlat +a=6378388 +rf=297.000000198989 +step "
        "+proj=molodensky +a=6378388 +rf=297.000000198989 +dx=-84.87 "
        "+dy=-96.49 +dz=-116.95 +da=-251 +df=-1.41927e-05 +step "
        "+proj=unitconvert +xy_in=rad +xy_out=deg");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_molodensky_abridged) {
    std::string projString("+proj=molodensky +ellps=WGS84 +dx=84.87 +dy=96.49 "
                           "+dz=116.95 +da=251 +df=1.41927e-05 +abridged");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);
    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
        "+step +proj=molodensky +ellps=WGS84 +dx=84.87 +dy=96.49 "
        "+dz=116.95 +da=251 +df=1.41927e-05 +abridged +step +proj=longlat "
        "+a=6378388 +rf=297.000000198989 +step "
        "+proj=unitconvert +xy_in=rad +xy_out=deg");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_molodensky_complex_pipeline) {
    std::string projString(
        "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
        "+proj=longlat +ellps=WGS84 +step +proj=molodensky +ellps=WGS84 "
        "+dx=84.87 +dy=96.49 "
        "+dz=116.95 +da=251 +df=1.41927e-05 +step +proj=longlat "
        "+ellps=GRS80 +step +proj=unitconvert "
        "+xy_in=rad +xy_out=deg");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto transf = nn_dynamic_pointer_cast<Transformation>(obj);
    ASSERT_TRUE(transf != nullptr);

    WKTFormatterNNPtr f(WKTFormatter::create());
    f->simulCurNodeHasId();
    f->setMultiLine(false);
    transf->exportToWKT(f.get());
    auto wkt = f->toString();
    EXPECT_TRUE(wkt.find("SOURCECRS[GEODCRS[\"unknown\",DATUM[\"Unknown based "
                         "on WGS84 ellipsoid\"") != std::string::npos)
        << wkt;
    EXPECT_TRUE(wkt.find("TARGETCRS[GEODCRS[\"unknown\",DATUM[\"Unknown based "
                         "on GRS80 ellipsoid\"") != std::string::npos)
        << wkt;

    EXPECT_EQ(
        transf->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_5)
                .get()),
        "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
        "+step +proj=molodensky +ellps=WGS84 +dx=84.87 +dy=96.49 "
        "+dz=116.95 +da=251 +df=1.41927e-05 +step +proj=unitconvert "
        "+xy_in=rad +xy_out=deg");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_title) {
    std::string projString("+title=Ile d'Amsterdam 1963 +proj=longlat "
                           "+towgs84=109.7530,-528.1330,-362.2440 "
                           "+a=6378388.0000 +rf=297.0000000000000 +units=m "
                           "+no_defs");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    auto baseCRS = nn_dynamic_pointer_cast<GeographicCRS>(crs->baseCRS());
    ASSERT_TRUE(baseCRS != nullptr);
    EXPECT_EQ(baseCRS->nameStr(), "Ile d'Amsterdam 1963");
    EXPECT_EQ(baseCRS->datum()->nameStr(), "Ile d'Amsterdam 1963");
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=longlat +ellps=intl +towgs84=109.753,-528.133,-362.244,0,0,0,0 "
        "+no_defs");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_projected_title) {
    std::string projString(
        "+title=Amsterdam 1963 +proj=tmerc "
        "+towgs84=109.7530,-528.1330,-362.2440 +a=6378388.0000 "
        "+rf=297.0000000000000 +lat_0=0.000000000 +lon_0=75.000000000 "
        "+k_0=0.99960000 +x_0=500000.000 +y_0=10000000.000 +units=m +no_defs");
    auto obj = PROJStringParser().createFromPROJString(projString);
    auto crs = nn_dynamic_pointer_cast<BoundCRS>(obj);
    ASSERT_TRUE(crs != nullptr);
    auto baseCRS = nn_dynamic_pointer_cast<ProjectedCRS>(crs->baseCRS());
    ASSERT_TRUE(baseCRS != nullptr);
    EXPECT_EQ(baseCRS->nameStr(), "Amsterdam 1963");
    EXPECT_EQ(baseCRS->baseCRS()->nameStr(), "unknown");
    EXPECT_EQ(
        crs->exportToPROJString(
            PROJStringFormatter::create(PROJStringFormatter::Convention::PROJ_4)
                .get()),
        "+proj=utm +zone=43 +south +ellps=intl "
        "+towgs84=109.753,-528.133,-362.244,0,0,0,0 +units=m +no_defs");
}

// ---------------------------------------------------------------------------

TEST(io, projparse_init) {
    auto dbContext = DatabaseContext::create();

    // Not allowed in non-compatibillity mode
    EXPECT_THROW(PROJStringParser().createFromPROJString("init=epsg:4326"),
                 ParsingException);

    {
        // EPSG:4326 is normally latitude-longitude order with degree,
        // but in compatibillity mode it will be long-lat radian
        auto obj = createFromUserInput("init=epsg:4326", dbContext, true);
        auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
        ASSERT_TRUE(crs != nullptr);
        EXPECT_TRUE(crs->coordinateSystem()->isEquivalentTo(
            EllipsoidalCS::createLongitudeLatitude(UnitOfMeasure::RADIAN)
                .get()));
    }

    {
        // EPSG:3040 is normally northing-easting order, but in compatibillity
        // mode it will be easting-northing
        auto obj = createFromUserInput("init=epsg:3040", dbContext, true);
        auto crs = nn_dynamic_pointer_cast<ProjectedCRS>(obj);
        ASSERT_TRUE(crs != nullptr);
        EXPECT_TRUE(crs->coordinateSystem()->isEquivalentTo(
            CartesianCS::createEastingNorthing(UnitOfMeasure::METRE).get()));
    }

    {
        auto obj =
            PROJStringParser().createFromPROJString("init=ITRF2000:ITRF2005");
        auto co = nn_dynamic_pointer_cast<CoordinateOperation>(obj);
        ASSERT_TRUE(co != nullptr);
        EXPECT_EQ(co->exportToPROJString(PROJStringFormatter::create().get()),
                  "+proj=helmert +x=-0.0001 +y=0.0008 +z=0.0058 +rx=0 +ry=0 "
                  "+rz=0 +s=-0.0004 +dx=0.0002 +dy=-0.0001 +dz=0.0018 +drx=0 "
                  "+dry=0 +drz=0 +ds=-8e-06 +t_epoch=2000 "
                  "+convention=position_vector");
    }

    {
        auto obj = createFromUserInput(
            "title=mytitle init=epsg:4326 ellps=WGS84", dbContext, true);
        auto co = nn_dynamic_pointer_cast<CoordinateOperation>(obj);
        ASSERT_TRUE(co != nullptr);
        EXPECT_EQ(co->nameStr(), "mytitle");
        EXPECT_EQ(co->exportToPROJString(PROJStringFormatter::create().get()),
                  "+init=epsg:4326 +ellps=WGS84");
    }

    {
        auto obj = createFromUserInput(
            "proj=pipeline step init=epsg:4326 step proj=longlat ellps=WGS84",
            dbContext, true);
        auto co = nn_dynamic_pointer_cast<CoordinateOperation>(obj);
        ASSERT_TRUE(co != nullptr);
        EXPECT_EQ(co->exportToPROJString(PROJStringFormatter::create().get()),
                  "+proj=pipeline +step +init=epsg:4326 +step +proj=longlat "
                  "+ellps=WGS84");
    }

    {
        auto obj = createFromUserInput(
            "init=epsg:4326 proj=longlat ellps=GRS80", dbContext, true);
        auto crs = nn_dynamic_pointer_cast<GeographicCRS>(obj);
        ASSERT_TRUE(crs != nullptr);
        EXPECT_EQ(crs->exportToPROJString(PROJStringFormatter::create().get()),
                  "+proj=pipeline +step +proj=longlat +ellps=GRS80 "
                  "+step +proj=unitconvert +xy_in=rad +xy_out=deg");
    }
}

// ---------------------------------------------------------------------------

TEST(io, projparse_errors) {
    EXPECT_THROW(PROJStringParser().createFromPROJString(""), ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString("foo"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString("inv"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString("step"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString("proj=unknown"),
                 ParsingException);

    EXPECT_THROW(
        PROJStringParser().createFromPROJString(
            "proj=pipeline step proj=unitconvert step proj=longlat a=invalid"),
        ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "proj=pipeline step proj=pipeline"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "proj=pipeline step init=epsg:4326 init=epsg:4326"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_longlat_errors) {
    EXPECT_THROW(
        PROJStringParser().createFromPROJString("+proj=longlat +datum=unknown"),
        ParsingException);

    EXPECT_THROW(
        PROJStringParser().createFromPROJString("+proj=longlat +ellps=unknown"),
        ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=longlat +a=invalid +b=1"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=longlat +a=1 +b=invalid"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=longlat +a=invalid +rf=1"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=longlat +a=1 +rf=invalid"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=longlat +a=1 +f=invalid"),
                 ParsingException);

    EXPECT_THROW(
        PROJStringParser().createFromPROJString("+proj=longlat +R=invalid"),
        ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString("+proj=longlat +b=1"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString("+proj=longlat +rf=1"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString("+proj=longlat +f=0"),
                 ParsingException);

    EXPECT_THROW(
        PROJStringParser().createFromPROJString("+proj=longlat +pm=unknown"),
        ParsingException);

    EXPECT_THROW(
        PROJStringParser().createFromPROJString(
            "+proj=longlat +ellps=GRS80 +towgs84=1.2,2,3,4,5,6,invalid"),
        ParsingException);

    EXPECT_THROW(
        PROJStringParser().createFromPROJString("+proj=longlat +axis=foo"),
        ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
                     "+proj=unitconvert +xy_in=rad +xy_out=foo"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
                     "+proj=axisswap"),
                 ParsingException);

    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=pipeline +step +proj=longlat +ellps=GRS80 +step "
                     "+proj=axisswap +order=0,0"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(io, projparse_projected_errors) {
    EXPECT_THROW(
        PROJStringParser().createFromPROJString("+proj=tmerc +units=foo"),
        ParsingException);
    EXPECT_THROW(
        PROJStringParser().createFromPROJString("+proj=tmerc +x_0=foo"),
        ParsingException);
    EXPECT_THROW(
        PROJStringParser().createFromPROJString("+proj=tmerc +lat_0=foo"),
        ParsingException);
    // Inconsistent pm values between geogCRS and projectedCRS
    EXPECT_THROW(PROJStringParser().createFromPROJString(
                     "+proj=pipeline +step +proj=longlat +ellps=WGS84 "
                     "+proj=tmerc +ellps=WGS84  +lat_0=foo +pm=paris"),
                 ParsingException);
}

// ---------------------------------------------------------------------------

TEST(io, createFromUserInput) {
    auto dbContext = DatabaseContext::create();
    EXPECT_THROW(createFromUserInput("foo", nullptr), ParsingException);
    EXPECT_THROW(createFromUserInput("GEOGCRS", nullptr), ParsingException);
    EXPECT_THROW(createFromUserInput("+proj=unhandled", nullptr),
                 ParsingException);
    EXPECT_THROW(createFromUserInput("EPSG:4326", nullptr), ParsingException);
    EXPECT_THROW(
        createFromUserInput("urn:ogc:def:unhandled:EPSG::4326", dbContext),
        ParsingException);

    EXPECT_NO_THROW(createFromUserInput("+proj=longlat", nullptr));
    EXPECT_NO_THROW(createFromUserInput("EPSG:4326", dbContext));
    EXPECT_NO_THROW(createFromUserInput("epsg:4326", dbContext));
    EXPECT_NO_THROW(
        createFromUserInput("urn:ogc:def:crs:EPSG::4326", dbContext));
    EXPECT_NO_THROW(createFromUserInput(
        "urn:ogc:def:coordinateOperation:EPSG::1671", dbContext));
    EXPECT_NO_THROW(
        createFromUserInput("urn:ogc:def:datum:EPSG::6326", dbContext));
    EXPECT_NO_THROW(
        createFromUserInput("urn:ogc:def:meridian:EPSG::8901", dbContext));
    EXPECT_NO_THROW(
        createFromUserInput("urn:ogc:def:ellipsoid:EPSG::7030", dbContext));
    EXPECT_NO_THROW(createFromUserInput(
        "GEOGCRS[\"WGS 84\",\n"
        "    DATUM[\"World Geodetic System 1984\",\n"
        "        ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
        "    CS[ellipsoidal,3],\n"
        "        AXIS[\"latitude\",north,\n"
        "            UNIT[\"degree\",0.0174532925199433]],\n"
        "        AXIS[\"longitude\",east,\n"
        "            UNIT[\"degree\",0.0174532925199433]],\n"
        "        AXIS[\"ellipsoidal height\",up,\n"
        "            UNIT[\"metre\",1]],\n"
        "    ID[\"EPSG\",4979]]",
        nullptr));

    // Search names in the database
    EXPECT_THROW(createFromUserInput("foobar", dbContext), ParsingException);
    EXPECT_NO_THROW(createFromUserInput("WGS 84", dbContext));
    EXPECT_NO_THROW(createFromUserInput("WGS84", dbContext));
    EXPECT_NO_THROW(createFromUserInput("UTM zone 31N", dbContext));
    EXPECT_THROW(createFromUserInput("UTM zone 31", dbContext),
                 ParsingException);
    EXPECT_NO_THROW(createFromUserInput("WGS84 UTM zone 31N", dbContext));
}

// ---------------------------------------------------------------------------

TEST(io, guessDialect) {
    EXPECT_EQ(WKTParser().guessDialect("LOCAL_CS[\"foo\"]"),
              WKTParser::WKTGuessedDialect::WKT1_GDAL);

    EXPECT_EQ(WKTParser().guessDialect(
                  "GEOGCS[\"GCS_WGS_1984\",DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_"
                  "1984\",6378137.0,298.257223563]],PRIMEM[\"Greenwich\",0.0],"
                  "UNIT[\"Degree\",0.0174532925199433]]"),
              WKTParser::WKTGuessedDialect::WKT1_ESRI);

    EXPECT_EQ(WKTParser().guessDialect(
                  "GEOGCRS[\"WGS 84\",\n"
                  "    DATUM[\"World Geodetic System 1984\",\n"
                  "        ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
                  "    CS[ellipsoidal,2],\n"
                  "        AXIS[\"geodetic latitude (Lat)\",north],\n"
                  "        AXIS[\"geodetic longitude (Lon)\",east],\n"
                  "        UNIT[\"degree\",0.0174532925199433]]"),
              WKTParser::WKTGuessedDialect::WKT2_2018);

    EXPECT_EQ(
        WKTParser().guessDialect("TIMECRS[\"Temporal CRS\",\n"
                                 "    TDATUM[\"Gregorian calendar\",\n"
                                 "        CALENDAR[\"proleptic Gregorian\"],\n"
                                 "        TIMEORIGIN[0000-01-01]],\n"
                                 "    CS[TemporalDateTime,1],\n"
                                 "        AXIS[\"time (T)\",future]]"),
        WKTParser::WKTGuessedDialect::WKT2_2018);

    EXPECT_EQ(WKTParser().guessDialect(
                  "GEODCRS[\"WGS 84\",\n"
                  "    DATUM[\"World Geodetic System 1984\",\n"
                  "        ELLIPSOID[\"WGS 84\",6378137,298.257223563]],\n"
                  "    CS[ellipsoidal,2],\n"
                  "        AXIS[\"geodetic latitude (Lat)\",north],\n"
                  "        AXIS[\"geodetic longitude (Lon)\",east],\n"
                  "        UNIT[\"degree\",0.0174532925199433]]"),
              WKTParser::WKTGuessedDialect::WKT2_2015);

    EXPECT_EQ(WKTParser().guessDialect("foo"),
              WKTParser::WKTGuessedDialect::NOT_WKT);
}