// Copyright (c) Lawrence Livermore National Security, LLC and other Conduit
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Conduit.

//-----------------------------------------------------------------------------
///
/// file: conduit_blueprint_mpi_mesh_partition.cpp
///
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// conduit includes
//-----------------------------------------------------------------------------
#include "conduit_blueprint_mesh.hpp"
#include "conduit_blueprint_mpi_mesh.hpp"
#include "conduit_blueprint_mpi_mesh_partition.hpp"

#include "conduit_relay_mpi.hpp"
#include <mpi.h>
using partitioner = conduit::blueprint::mesh::partitioner;

using std::cout;
using std::endl;

// Uncomment these macros to enable debugging output.
//#define CONDUIT_DEBUG_MAP_CHUNKS
//#define CONDUIT_DEBUG_COMMUNICATE_CHUNKS

// Renumber domains in parallel
#define RENUMBER_DOMAINS

//-----------------------------------------------------------------------------
// -- begin conduit --
//-----------------------------------------------------------------------------
namespace conduit
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint --
//-----------------------------------------------------------------------------
namespace blueprint
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint::mpi --
//-----------------------------------------------------------------------------
namespace mpi
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint::mesh --
//-----------------------------------------------------------------------------

namespace mesh 
{
//---------------------------------------------------------------------------
parallel_partitioner::parallel_partitioner(MPI_Comm c) : partitioner()
{
    comm = c;
    MPI_Comm_size(comm, &size);
    MPI_Comm_rank(comm, &rank);
    create_chunk_info_dt();
}

//---------------------------------------------------------------------------
parallel_partitioner::~parallel_partitioner()
{
    free_chunk_info_dt();
}

//---------------------------------------------------------------------------
bool
parallel_partitioner::options_get_target(const conduit::Node &options,
    unsigned int &value) const
{
    // Get the target value using the base class method.
    unsigned int val = 0;
    partitioner::options_get_target(options, val);

    // Take the max target across ranks. Ranks that did not provide it use 0.
    MPI_Allreduce(&val, &value, 1, MPI_UNSIGNED, MPI_MAX, comm);

    // A target was set by at least one rank if we have a value greater than 0.
    return value > 0;
}

//---------------------------------------------------------------------------
long
parallel_partitioner::get_total_selections() const
{
    // Gather the number of selections on each rank.
    long nselections = static_cast<long>(selections.size());
    long ntotal_selections = nselections;
    MPI_Allreduce(&nselections, &ntotal_selections, 1, MPI_LONG, MPI_SUM, comm);

    return ntotal_selections;
}

//---------------------------------------------------------------------------
/**
 @note This method is called iteratively until we have the number of target
       selections that we want to make. We could do better by identifying
       more selections to split in each pass. We use MPI_LONG_INT because it
       can be used with MPI_MAXLOC.
 */
void
parallel_partitioner::get_largest_selection(int &sel_rank, int &sel_index) const
{
    // Find largest selection locally.
    long_int largest_selection;
    largest_selection.value = 0;
    largest_selection.rank = rank;
    std::vector<uint64> local_sizes(selections.size());
    for(size_t i = 0; i < selections.size(); i++)
    {
        local_sizes[i] = static_cast<uint64>(selections[i]->length());
        if(local_sizes[i] > static_cast<uint64>(largest_selection.value))
        {
            largest_selection.value = static_cast<long>(local_sizes[i]);
        }
    }

    // What's the largest selection across ranks? We use MPI_MAXLOC to 
    // get the max size and the rank where it occurs.
    long_int global_largest_selection;
    MPI_Allreduce(&largest_selection, &global_largest_selection,
                  1, MPI_LONG_INT, MPI_MAXLOC, comm);

    // MPI found us the rank that has the largest selection.
    sel_rank = global_largest_selection.rank;
    sel_index = -1;

    // If we're on that rank, determine the local selection index.
    if(sel_rank == rank)
    {
        uint64 ssize = static_cast<uint64>(global_largest_selection.value);
        for(size_t i = 0; i < selections.size(); i++)
        {
            if(ssize == local_sizes[i])
            {
                sel_index = static_cast<int>(i);
                break;
            }
        }
    }
}

//-------------------------------------------------------------------------
void
parallel_partitioner::create_chunk_info_dt()
{
    chunk_info obj;
    int slen = 3;
    int lengths[3] = {1,1,1};
    MPI_Datatype types[3];
    MPI_Aint offsets[3];

    types[0] = MPI_UNSIGNED_LONG_LONG;
    types[1] = MPI_INT;
    types[2] = MPI_INT;

    size_t base = ((size_t)(&obj));
    offsets[0] = ((size_t)(&obj.num_elements)) - base;
    offsets[1] = ((size_t)(&obj.destination_rank)) - base;
    offsets[2] = ((size_t)(&obj.destination_domain)) - base;

    MPI_Type_create_struct(slen, lengths, offsets, types, &chunk_info_dt);
    MPI_Type_commit(&chunk_info_dt);
}

//-------------------------------------------------------------------------
void
parallel_partitioner::free_chunk_info_dt()
{
    MPI_Type_free(&chunk_info_dt);
}

//-------------------------------------------------------------------------
/**
The purpose of this function is to decide for a set of chunks on a rank, how
they will be assigned to final domains and where in the set of MPI ranks those
domains will live.

Some chunks will not care which domain they belong to nor where they
might end up. Such chunks will indicate a -1 for their domain number so we
have some freedom in how we assemble chunks into domains, according to the
target number of domains.

Some chunks may be the result of a field-based selection that
says explicitly where the cells will end up in a domain/rank. We can only have
a domain going to a single rank though.

@note We pass out the global dest_rank, dest_domain, offsets in this method
      since we consume it immediately in communicate_chunks where we need the
      global information to do matching sends/recvs.
*/
void
parallel_partitioner::map_chunks(const std::vector<partitioner::chunk> &chunks,
    std::vector<int> &dest_rank,
    std::vector<int> &dest_domain,
    std::vector<int> &_offsets)
{
    // Gather number of chunks on each rank.
    auto nlocal_chunks = static_cast<int>(chunks.size());
    std::vector<int> nglobal_chunks(size, 0);
    MPI_Allgather(&nlocal_chunks, 1, MPI_INT,
                  &nglobal_chunks[0], 1, MPI_INT,
                  comm);
    // Compute total chunks
    int ntotal_chunks = 0;
    for(size_t i = 0; i < nglobal_chunks.size(); i++)
        ntotal_chunks += nglobal_chunks[i];

#ifdef CONDUIT_DEBUG_MAP_CHUNKS
    MPI_Barrier(comm);
    if(rank == 0)
    {
        cout << "------------------------ map_chunks ------------------------" << endl;
        cout << "ntotal_chunks = " << ntotal_chunks << endl;
    } 
    MPI_Barrier(comm);
#endif

    // Compute offsets. We use int because of MPI_Allgatherv
    std::vector<int> offsets(size, 0);
    for(size_t i = 1; i < nglobal_chunks.size(); i++)
        offsets[i] = offsets[i-1] + nglobal_chunks[i-1];

    // What we have at this point is a list of chunk sizes for all chunks
    // across all ranks. Let's get a global list of chunk domains (where
    // they want to go). A chunk may already know where it wants to go.
    // If it doesn't then we can assign it to move around. A chunk is
    // free to move around if its destination domain is -1.
    std::vector<chunk_info> local_chunk_info(chunks.size());
    for(size_t i = 0; i < chunks.size(); i++)
    {
        const conduit::Node &n_topos = chunks[i].mesh->operator[]("topologies");
        uint64 len = 0;
        for(index_t j = 0; j < n_topos.number_of_children(); j++)
            len += conduit::blueprint::mesh::topology::length(n_topos[j]);
        local_chunk_info[i].num_elements = len;
        local_chunk_info[i].destination_rank   = chunks[i].destination_rank;
        local_chunk_info[i].destination_domain = chunks[i].destination_domain;
    }
    std::vector<chunk_info> global_chunk_info(ntotal_chunks);
    MPI_Allgatherv(&local_chunk_info[0],
                   static_cast<int>(local_chunk_info.size()),
                   chunk_info_dt,
                   &global_chunk_info[0],
                   &nglobal_chunks[0],
                   &offsets[0],
                   chunk_info_dt,
                   comm);
#ifdef CONDUIT_DEBUG_MAP_CHUNKS
    if(rank == 0)
    {
        for(int i = 0; i < ntotal_chunks; i++)
        {
            cout << "global_chunk_info[" << i << "]={"
                 << "num_elements=" << global_chunk_info[i].num_elements
                 << ", dest_rank=" << global_chunk_info[i].destination_rank
                 << ", dest_domain=" << global_chunk_info[i].destination_domain
                 << "}" << endl;
        }
    }
#endif

    // Determine how many ranks are free to move to various domains.
    // Also determine the domain ids in use and how many chunks
    // comprise each of them.
    std::map<int,int> domain_sizes;
    int free_to_move = 0;
    for(int i = 0; i < ntotal_chunks; i++)
    {
        int domid = global_chunk_info[i].destination_domain;
        if(domid >= 0)
        {
            std::map<int,int>::iterator it = domain_sizes.find(domid);
            if(it == domain_sizes.end())
                domain_sizes[domid] = 1;
            else
                it->second++;
        }
        else
            free_to_move++;
    }

#ifdef CONDUIT_DEBUG_MAP_CHUNKS
    if(rank == 0)
    {
        cout << "domain_sizes = {";
        for(std::map<int,int>::const_iterator it = domain_sizes.begin();
            it != domain_sizes.end(); it++)
        {
            cout << it->first << ": " << it->second << ", ";
        }
        cout << "}" << endl;
        cout << "free_to_move = " << free_to_move << endl;
    }
#endif

    if(free_to_move == 0)
    {
        // No chunks are free to move around. This means we the domains
        // that we want them all to belong to.

        // NOTE: This may mean that we do not get #target domains though.
        if(!domain_sizes.empty() && domain_sizes.size() != target)
        {
            CONDUIT_WARN("The unique number of domain ids "
                << domain_sizes.size()
                << " was not equal to the desired target number of domains: "
                << target  << ".");
        }

        // Pass out global information
        dest_rank.reserve(ntotal_chunks);
        dest_domain.reserve(ntotal_chunks);
        for(int i = 0; i < ntotal_chunks; i++)
        {
            dest_rank.push_back(global_chunk_info[i].destination_rank);
            dest_domain.push_back(global_chunk_info[i].destination_domain);
        }
        _offsets.swap(offsets);
    }
    else if(free_to_move == ntotal_chunks)
    {
        // No chunks told us where they go so ALL are free to move.
#ifdef CONDUIT_DEBUG_MAP_CHUNKS
        if(rank == 0)
        {
            cout << "** We decide where chunks go." << endl;
        }
#endif
        // We must make #target domains from the chunks we have. Since
        // no chunks told us a domain id they want to be inside, we can
        // number domains 0..target. This scheme ignores the chunk's
        // destination_rank.

        std::vector<uint64> target_element_counts(target, 0);
        std::vector<uint64> next_target_element_counts(target);
        std::vector<int> global_dest_rank(ntotal_chunks, -1);
        std::vector<int> global_dest_domain(ntotal_chunks, 0);
        for(int i = 0; i < ntotal_chunks; i++)
        {
            // Add the size of this chunk to all targets.
            for(unsigned int r = 0; r < target; r++)
            {
                next_target_element_counts[r] = target_element_counts[r] +
                                                global_chunk_info[i].num_elements;
            }

            // Find the index of the min value in next_target_element_counts.
            // This way, we'll let that target domain have the chunk as
            // we are trying to balance the sizes of the output domains.
            //
            // NOTE: We could consider other metrics too such as making the
            //       smallest bounding box so we keep things close together.
            //
            // NOTE: This method has the potential to move chunks far away.
            //       It is sprinkling chunks into targets 0,1,2,... and 
            //       and then repeating when the number of elements is ascending.
            int idx = 0;
            for(unsigned int r = 1; r < target; r++)
            {
                if(next_target_element_counts[r] < next_target_element_counts[idx])
                    idx = r;
            }

            // Add the current chunk to the specified target domain.
            target_element_counts[idx] += global_chunk_info[i].num_elements;
            global_dest_domain[i] = idx;
#ifdef CONDUIT_DEBUG_MAP_CHUNKS
            if(rank == 0)
            {
                cout << "Add chunk " << i << " to domain " << idx
                     << " (nelem=" << target_element_counts[idx] << ")" << endl;
            }
#endif
        }

        // We now have a global map indicating the final domain to which
        // each chunk will contribute. Spread target domains across size ranks.
        std::vector<int> rank_domain_count(size, 0);
        int divsize = std::min(size, static_cast<int>(target));
        for(unsigned int i = 0; i < target; i++)
            rank_domain_count[i % divsize]++;

        // Figure out which source chunks join which ranks.
        int target_id = 0;
        for(int r = 0; r < size; r++)
        {
#ifdef CONDUIT_DEBUG_MAP_CHUNKS
            if(rank == 0)
            {
                cout << "r=" << r << ", rank_domain_count[r]="
                     << rank_domain_count[r] << endl;
            }
#endif
            if(rank_domain_count[r] == 0)
                break;
            // For each domain on this rank r.
            for(int j = 0; j < rank_domain_count[r]; j++)
            {
                for(int i = 0; i < ntotal_chunks; i++)
                {
                    if(global_dest_domain[i] == target_id)
                    {
#ifdef CONDUIT_DEBUG_MAP_CHUNKS
                        if(rank == 0)
                        {
                            cout << "global domain " << target_id
                                 << " goes to " << r << endl;
                        }
#endif
                        global_dest_rank[i] = r;
                    }
                }
                target_id++;
            }
        }
#ifdef CONDUIT_DEBUG_MAP_CHUNKS
        if(rank == 0)
        {
            cout << "target=" << target << endl;
            cout << "target_element_counts = {";
            for(size_t i = 0; i < target_element_counts.size(); i++)
                cout << target_element_counts[i] << ", ";
            cout << "}" << endl;
            cout << "global_dest_rank = {";
            for(size_t i = 0; i < global_dest_rank.size(); i++)
                cout << global_dest_rank[i] << ", ";
            cout << "}" << endl;
            cout << "global_dest_domain = {";
            for(size_t i = 0; i < global_dest_domain.size(); i++)
                cout << global_dest_domain[i] << ", ";
            cout << "}" << endl;
            cout << "rank_domain_count = {";
            for(size_t i = 0; i < rank_domain_count.size(); i++)
                cout << rank_domain_count[i] << ", ";
            cout << "}" << endl;
        }
#endif
        // Pass out the global information.
        dest_rank.swap(global_dest_rank);
        dest_domain.swap(global_dest_domain);
        _offsets.swap(offsets);
    }
    else
    {
        // There must have been a combination of chunks that told us where
        // they want to go and some that did not. Determine whether we want
        // to handle this.
        CONDUIT_ERROR("Invalid mixture of destination rank/domain specifications.");
    }
#ifdef CONDUIT_DEBUG_MAP_CHUNKS
    // We're passing out global info now so all ranks should be the same.
    if(rank == 0)
    {
        std::cout << rank << ": dest_ranks={";
        for(size_t i = 0; i < dest_rank.size(); i++)
            std::cout << dest_rank[i] << ", ";
        std::cout << "}" << std::endl;
        std::cout << rank << ": dest_domain={";
        for(size_t i = 0; i < dest_domain.size(); i++)
            std::cout << dest_domain[i] << ", ";
        std::cout << "}" << std::endl;
        std::cout.flush();
    }
#endif
}

//-------------------------------------------------------------------------
/**
 @note In the parallel version of this function, we pass in global information
       for dest_rank, dest_domain, offsets. This helps us know not only the
       domains to which we have to send but also those who are sending to
       this rank.
 */
void
parallel_partitioner::communicate_chunks(const std::vector<partitioner::chunk> &chunks,
    const std::vector<int> &dest_rank,
    const std::vector<int> &dest_domain,
    const std::vector<int> &offsets,
    std::vector<partitioner::chunk> &chunks_to_assemble,
    std::vector<int> &chunks_to_assemble_domains)
{
    const int PARTITION_TAG_BASE = 12000;

    // Use the offsets to determine the sender for each chunk.
    std::vector<int> src_rank(dest_rank.size(),size-1);
    size_t idx = 0;
    for(size_t r = 1; r < offsets.size(); r++)
    {
        int n = offsets[r] - offsets[r-1];
        for(int i = 0; i < n; i++)
            src_rank[idx++] = r-1;
    }

#ifdef CONDUIT_DEBUG_COMMUNICATE_CHUNKS
    MPI_Barrier(comm);
    if(rank == 0)
    {
        cout << "offsets = {";
        for(size_t i = 0; i < offsets.size(); i++)
            cout << offsets[i] << ", ";
        cout << "}" << endl;
        cout << "src_rank = {";
        for(size_t i = 0; i < src_rank.size(); i++)
            cout << src_rank[i] << ", ";
        cout << "}" << endl;
    }
    MPI_Barrier(comm);
#endif

    // Do sends for the chunks we own on this processor that must migrate.
    for(size_t i = 0; i < chunks.size(); i++)
    {
        int gidx = offsets[rank] + i;
        int tag = PARTITION_TAG_BASE + gidx;
        int dest = dest_rank[gidx];
        // If we're not sending to self, send the chunk.
        if(dest != rank)
        {
#ifdef CONDUIT_DEBUG_COMMUNICATE_CHUNKS
            cout << rank << ": send_using_schema(dest=" << dest
                 << ", tag=" << tag << ")" << endl;
#endif
            conduit::relay::mpi::send_using_schema(*chunks[i].mesh, dest, tag, comm);
        }
    }

    // Do recvs.
    for(size_t i = 0; i < dest_rank.size(); i++)
    {
        if(dest_rank[i] == rank)
        {
            int gidx = i;
            int tag = PARTITION_TAG_BASE + gidx;
            int start = offsets[rank];
            int end = start + chunks.size();
            bool this_rank_already_owns_it = (gidx >= start && gidx < end);
            if(this_rank_already_owns_it)
            {
                int local_i = i - offsets[rank];

#ifdef RENUMBER_DOMAINS
                // The chunk we have here needs its state/domain_id updated but
                // we really should not modify it directly. Do we have to make
                // a new node and set_external everything in it? Then make it
                // have its own state/domain_id?
                conduit::Node *n_recv = new conduit::Node;
                for(index_t ci = 0; ci < chunks[local_i].mesh->number_of_children(); ci++)
                {
                    const conduit::Node &n = chunks[local_i].mesh->operator[](ci);
                    if(n.name() != "state")
                        (*n_recv)[n.name()].set_external_node(n);
                }
                if(chunks[local_i].mesh->has_path("state/cycle"))
                    (*n_recv)["state/cycle"] = (*chunks[local_i].mesh)["state/cycle"];
                if(chunks[local_i].mesh->has_path("state/time"))
                    (*n_recv)["state/time"] = (*chunks[local_i].mesh)["state/time"];
                (*n_recv)["state/domain_id"] = i;

                // Save the chunk "wrapper" that has its own state.
                chunks_to_assemble.push_back(chunk(n_recv, true));
                chunks_to_assemble_domains.push_back(dest_domain[i]);
#else
                // Pass the chunk through since we already own it on this rank.
                chunks_to_assemble.push_back(chunk(chunks[local_i].mesh, false));
                chunks_to_assemble_domains.push_back(dest_domain[i]);
#endif
            }
            else
            {
#ifdef CONDUIT_DEBUG_COMMUNICATE_CHUNKS
                cout << rank << ": recv_using_schema(src=" << src_rank[i]
                     << ", tag=" << tag << ")" << endl;
#endif
                conduit::Node *n_recv = new conduit::Node;              
                conduit::relay::mpi::recv_using_schema(*n_recv, src_rank[i], tag, comm);

#ifdef RENUMBER_DOMAINS
                // Since we had to receive the chunk, we can patch up its state/domain_id
                // to the updated numbering scheme.
                (*n_recv)["state/domain_id"] = i;
#endif
                // Save the received chunk and indicate we own it for later.
                chunks_to_assemble.push_back(chunk(n_recv, true));
                chunks_to_assemble_domains.push_back(dest_domain[i]);
            }
        }
    }
}

//-----------------------------------------------------------------------------
}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint::mpi::mesh --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint::mpi --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint --
//-----------------------------------------------------------------------------

}
//-----------------------------------------------------------------------------
// -- end conduit:: --
//-----------------------------------------------------------------------------
