#ifndef POINT_MERGE_HPP
#define POINT_MERGE_HPP

// NOTE: THIS CLASS WILL BE MOVED

//-----------------------------------------------------------------------------
// conduit lib includes
//-----------------------------------------------------------------------------
#include "conduit.hpp"
#include "conduit_blueprint_exports.h"

//-----------------------------------------------------------------------------
// std lib includes
//-----------------------------------------------------------------------------
#include <tuple>
#include <map>

//-----------------------------------------------------------------------------
// -- begin conduit:: --
//-----------------------------------------------------------------------------
namespace conduit
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint --
//-----------------------------------------------------------------------------
namespace blueprint
{

class CONDUIT_BLUEPRINT_API point_merge
{
public:
    enum class coord_system
    {
        cartesian,
        cylindrical,
        spherical
    };


    void execute(const std::vector<conduit::Node *> coordsets, double tolerance,
                Node &output);

private:
    static double determine_scale(double tolerance);

    void iterate_coordinates(index_t domain_id, coord_system cs,
        const Node *xnode, const Node *ynode, const Node *znode);
    
    void insert(index_t dom_id, index_t pid, coord_system system,
                double x, double y = 0., double z = 0.);
    

    struct record
    {
        std::vector<index_t> orig_domains;
        std::vector<index_t> orig_ids;
    };
    using fp_type = std::int64_t;
    using tup = std::tuple<fp_type, fp_type, fp_type>;
    using point_records_type = std::map<tup, record>;
    point_records_type point_records;
    double scale;
    coord_system out_system;
};

//-----------------------------------------------------------------------------
}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit:: --
//-----------------------------------------------------------------------------

// cpp
#include <cmath>
#include <conduit_node.hpp>

//-----------------------------------------------------------------------------
// -- begin conduit:: --
//-----------------------------------------------------------------------------
namespace conduit
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint --
//-----------------------------------------------------------------------------
namespace blueprint
{

inline void 
point_merge::execute(const std::vector<Node *> coordsets, double tolerance,
                    Node &output)
{
    scale = determine_scale(tolerance);

    // Determine best output coordinate system
    out_system = coord_system::cartesian;
    const index_t ncoordsets = coordsets.size();
    for(index_t i = 0u; i < ncoordsets; i++)
    {
        const Node *coordset = coordsets[i]->fetch_ptr("values");
        // TODO: Check if it is explicit
        coord_system cs = coord_system::cartesian;
        const Node *xnode = coordset->fetch_ptr("x");
        const Node *ynode = nullptr, *znode = nullptr;
        if(xnode)
        {
            // Cartesian
            ynode = coordset->fetch_ptr("y");
            znode = coordset->fetch_ptr("z");
        }
        else if((xnode = coordset->fetch_ptr("r")))
        {
            if((ynode = coordset->fetch_ptr("z")))
            {
                // Cylindrical
                cs = coord_system::cylindrical;
            }
            else if((ynode = coordset->fetch_ptr("theta")))
            {
                // Spherical
                cs = coord_system::spherical;
                znode = coordset->fetch_ptr("phi");
            }
            else
            {
                // ERROR: Invalid coordinate system
                continue;
            }
        }
        // Is okay with nodes being nullptr
        iterate_coordinates(i, cs, xnode, ynode, znode);
    }

    // Iterate the record map
    const auto npoints = point_records.size();
    output.reset();
    auto &coordset = output.add_child("coordsets");
    auto &coords = coordset.add_child("coords");
    coords["type"] = "explicit";
    auto &values = coords.add_child("values");
    
    // create x,y,z
    Schema s;
    const index_t stride = sizeof(double) * 3;
    const index_t size = sizeof(double);
    s["x"].set(DataType::c_double(npoints,0,stride));
    s["y"].set(DataType::c_double(npoints,size,stride));
    s["z"].set(DataType::c_double(npoints,size*2,stride));
    
    // init the output
    values.set(s);
    double_array x_a = values["x"].value();
    double_array y_a = values["y"].value();
    double_array z_a = values["z"].value();
    auto itr = point_records.begin();
    std::cout << npoints << std::endl;
    for(size_t i = 0; i < npoints; i++, itr++)
    {
        const auto pair = *itr;
        x_a[i] = (double)std::get<0>(pair.first) / (double)scale;
        y_a[i] = (double)std::get<1>(pair.first) / (double)scale;
        z_a[i] = (double)std::get<2>(pair.first) / (double)scale;
    }
    // output.print();
}

inline double
point_merge::determine_scale(double)
{
    auto decimal_places = 4u;
    double retval = 0.;
    static const std::array<double, 7u> lookup = {
        1.,
        (2u << 4),
        (2u << 7),
        (2u << 10),
        (2u << 14),
        (2u << 17),
        (2u << 20)
    };
    if(decimal_places < lookup.size())
    {
        retval = lookup[decimal_places];
    }
    else
    {
        retval = lookup[6];
    }
    return retval;
}

inline void
point_merge::iterate_coordinates(index_t domain_id, coord_system cs,
    const Node *xnode, const Node *ynode, const Node *znode)
{
    if(xnode && ynode && znode)
    {
        // 3D
        const auto xtype = xnode->dtype();
        const auto ytype = ynode->dtype();
        const auto ztype = znode->dtype();
        // TODO: Handle different types
        auto xarray = xnode->as_double_array();
        auto yarray = ynode->as_double_array();
        auto zarray = znode->as_double_array();
        const index_t N = xarray.number_of_elements();
        for(index_t i = 0; i < N; i++)
        {
            insert(domain_id, i, cs, xarray[i], yarray[i], zarray[i]);
        }
    }
    else if(xnode && ynode)
    {
        // 2D
        const auto xtype = xnode->dtype();
        const auto ytype = ynode->dtype();
        // TODO: Handle different types
        auto xarray = xnode->as_double_array();
        auto yarray = ynode->as_double_array();
        const index_t N = xarray.number_of_elements();
        for(index_t i = 0; i < N; i++)
        {
            insert(domain_id, i, cs, xarray[i], yarray[i], 0.);
        }
    }
    else if(xnode)
    {
        // 1D
        const auto xtype = xnode->dtype();
        // TODO: Handle different types
        auto xarray = xnode->as_double_array();
        const index_t N = xarray.number_of_elements();
        for(index_t i = 0; i < N; i++)
        {
            insert(domain_id, i, cs, xarray[i], 0., 0.);
        }
    }
    else
    {
        // ERROR! No valid nodes passed.
    }
}

inline void 
point_merge::insert(index_t dom_id, index_t pid, coord_system system,
                    double x, double y, double z)
{
    tup key;
    // Translate to correct system
    switch(system)
    {
    case coord_system::cartesian: // TODO: Implement other systems
    default:
        switch(out_system)
        {
        case coord_system::cartesian: // TODO: Implement other systems
        default:
        {
            key = std::make_tuple(
                static_cast<fp_type>(std::round(x * scale)),
                static_cast<fp_type>(std::round(y * scale)),
                static_cast<fp_type>(std::round(z * scale)));
            break;
        }
        }
        break;
    }

    auto res = point_records.insert({key, {}});
    if(res.first != point_records.end())
    {
        auto &r = res.first->second;
        r.orig_domains.push_back(dom_id);
        r.orig_ids.push_back(pid);
    }
}

//-----------------------------------------------------------------------------
}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit:: --
//-----------------------------------------------------------------------------




#endif