//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2014-2015, Lawrence Livermore National Security, LLC.
// 
// Produced at the Lawrence Livermore National Laboratory
// 
// LLNL-CODE-666778
// 
// All rights reserved.
// 
// This file is part of Conduit. 
// 
// For details, see https://lc.llnl.gov/conduit/.
// 
// Please also read conduit/LICENSE
// 
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, 
//   this list of conditions and the disclaimer below.
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the disclaimer (as noted below) in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of the LLNS/LLNL nor the names of its contributors may
//   be used to endorse or promote products derived from this software without
//   specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY,
// LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
// DAMAGES  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE.
// 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

//-----------------------------------------------------------------------------
///
/// file: conduit_io.cpp
///
//-----------------------------------------------------------------------------

#include "conduit_io.hpp"

//-----------------------------------------------------------------------------
// standard lib includes
//-----------------------------------------------------------------------------
#include <iostream>

//-----------------------------------------------------------------------------
// -- begin conduit:: --
//-----------------------------------------------------------------------------
namespace conduit
{

    //-----------------------------------------------------------------------------
// -- begin conduit::io --
//-----------------------------------------------------------------------------
namespace io
{


//---------------------------------------------------------------------------//
void
identify_protocol(const std::string &path,
                  std::string &io_type)
{
    io_type = "conduit_bin";

    std::string file_path;
    std::string obj_base;

    // check for ":" split
    conduit::utils::split_string(path,
                                 std::string(":"),
                                 file_path,
                                 obj_base);

    if(obj_base.size() != 0)
    {
        std::string file_name_base;
        std::string file_name_ext;

        // find file extension to auto match
        conduit::utils::rsplit_string(file_path,
                                      std::string("."),
                                      file_name_base,
                                      file_name_ext);

        if(file_name_ext == "silo")
        {
            io_type = "conduit_silo";
        }
    }
}

//---------------------------------------------------------------------------//
void 
save(Node &node,
     const std::string &path)
{
    std::string protocol;
    identify_protocol(path,protocol);
    save(protocol,node,path);
}

//---------------------------------------------------------------------------//
void 
load(const std::string &path,
     Node &node)
{
    std::string protocol;
    identify_protocol(path,protocol);
    load(protocol,path,node);
}


//---------------------------------------------------------------------------//
void 
save(const std::string &protocol,
     Node &node,
     const std::string &path)
{
    if(protocol == "conduit_bin")
    {
        node.save(path);
    }
    else if( protocol == "conduit_silo")
    {
#ifdef CONDUIT_IO_ENABLE_SILO
        silo_save(node,path);
#else
        CONDUIT_ERROR("conduit_io lacks Silo support: " << 
                      "Failed to save conduit node to path " << path);
#endif
    }
    else if(protocol == "conduit_silo_mesh")
    {
#ifdef CONDUIT_IO_ENABLE_SILO
        silo_save_mesh(node,path);
#else
        CONDUIT_ERROR("conduit_io lacks Silo support: " << 
                      "Failed to save conduit mesh node to path " << path);
#endif
    }
    else
    {
        CONDUIT_ERROR("conduit_io unknown protocol: " << protocol);
    }
}

//---------------------------------------------------------------------------//
void
load(const std::string &protocol,
     const std::string &path,
     Node &node)
{

    if(protocol == "conduit_bin")
    {
        node.load(path);
    }
    else if( protocol == "conduit_silo")
    {
#ifdef CONDUIT_IO_ENABLE_SILO
        silo_load(path,node);
#else
        CONDUIT_ERROR("conduit_io lacks Silo support: " << 
                    "Failed to load conduit node from path " << path);
#endif
    }
    else if(protocol == "conduit_silo_mesh")
    {
        CONDUIT_ERROR("the conduit_io conduit_silo_mesh protocol does not "
                      "support \"load\"");
    }
    else
    {
        CONDUIT_ERROR("conduit_io unknown protocol: " << protocol);
        
    }

}

//---------------------------------------------------------------------------//
std::string
about()
{
    Node n;
    io::about(n);
    return n.to_json();
}

//---------------------------------------------------------------------------//
void
about(Node &n)
{
    n.reset();
    Node &protos = n["protocols"];

    // standard binary io
    protos["conduit_bin"] = "enabled";
    
    // rest server
    protos["rest"] = "enabled";
    
    // silo
#ifdef CONDUIT_IO_ENABLE_SILO
    protos["conduit_silo"] = "enabled";
#else
    protos["conduit_silo"] = "disabled";
#endif
    
    // silo mesh aware
#ifdef CONDUIT_IO_ENABLE_SILO
    protos["conduit_silo_mesh"] = "enabled";
#else
    protos["conduit_silo_mesh"] = "disabled";
#endif

}


};
//-----------------------------------------------------------------------------
// -- end conduit::io --
//-----------------------------------------------------------------------------



};
//-----------------------------------------------------------------------------
// -- end conduit:: --
//-----------------------------------------------------------------------------


