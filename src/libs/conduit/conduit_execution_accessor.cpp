// Copyright (c) Lawrence Livermore National Security, LLC and other Conduit
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Conduit.

//-----------------------------------------------------------------------------
///
/// file: conduit_execution_accessor.cpp
///
//-----------------------------------------------------------------------------
#include "conduit_execution_accessor.hpp"

//-----------------------------------------------------------------------------
// -- standard includes -- 
//-----------------------------------------------------------------------------
#include <algorithm>
#include <limits>
#include <type_traits>

//-----------------------------------------------------------------------------
// -- conduit includes -- 
//-----------------------------------------------------------------------------
#include "conduit_memory_manager.hpp"

//-----------------------------------------------------------------------------
// -- begin conduit:: --
//-----------------------------------------------------------------------------
namespace conduit
{

//-----------------------------------------------------------------------------
//
// -- conduit::ExecutionAccessor public methods --
//
//-----------------------------------------------------------------------------

//---------------------------------------------------------------------------//
template <typename T> 
ExecutionAccessor<T>::ExecutionAccessor()
: m_node_ptr(nullptr),
  m_other_ptr(nullptr),
  m_other_dtype(DataType::empty()),
  m_do_i_own_it(false),
  m_data(nullptr),
  m_offset(0),
  m_stride(0)
{}

//---------------------------------------------------------------------------//
template <typename T>
ExecutionAccessor<T>::ExecutionAccessor(const ExecutionAccessor<T> &accessor)
: m_node_ptr(accessor.m_node_ptr),
  m_other_ptr(accessor.m_other_ptr),
  m_other_dtype(accessor.m_other_dtype),
  m_do_i_own_it(accessor.m_do_i_own_it),
  m_data(accessor.m_data),
  m_offset(accessor.m_offset),
  m_stride(accessor.m_stride)
{}

//---------------------------------------------------------------------------//
template <typename T> 
ExecutionAccessor<T>::ExecutionAccessor(Node &node)
: m_node_ptr(&node),
  m_other_ptr(nullptr),
  m_other_dtype(DataType::empty()),
  m_do_i_own_it(false),
  m_data(node.data_ptr()),
  m_offset(node.dtype().offset()),
  m_stride(node.dtype().stride())
{}


//---------------------------------------------------------------------------//
template <typename T> 
ExecutionAccessor<T>::ExecutionAccessor(Node *node)
: m_node_ptr(node),
  m_other_ptr(nullptr),
  m_other_dtype(DataType::empty()),
  m_do_i_own_it(false),
  m_data(node->data_ptr()),
  m_offset(node->dtype().offset()),
  m_stride(node->dtype().stride())
{}


//---------------------------------------------------------------------------//
template <typename T> 
ExecutionAccessor<T>::ExecutionAccessor(const Node *node)
: m_node_ptr(const_cast<Node*>(node)),
  m_other_ptr(nullptr),
  m_other_dtype(DataType::empty()),
  m_do_i_own_it(false),
  m_data(const_cast<void*>(node->data_ptr())),
  m_offset(node->dtype().offset()),
  m_stride(node->dtype().stride())
{}


//---------------------------------------------------------------------------//
template <typename T> 
ExecutionAccessor<T>::~ExecutionAccessor()
{
    if (m_do_i_own_it)
    {
        if (DeviceMemory::is_device_ptr(m_other_ptr))
        {
            DeviceMemory::deallocate(m_other_ptr);
        }
        else
        {
            HostMemory::deallocate(m_other_ptr);
        }
    }
}


//---------------------------------------------------------------------------// 
///
/// Summary Stats Helpers
///
//---------------------------------------------------------------------------// 

//---------------------------------------------------------------------------// 
template <typename T>
T
ExecutionAccessor<T>::min()  const
{
    T res = std::numeric_limits<T>::max();
    for(index_t i = 0; i < number_of_elements(); i++)
    {
        const T &val = element(i);
        if(val < res)
        {
            res = val;
        }
    }

    return res;
}

//---------------------------------------------------------------------------// 
template <typename T>
T
ExecutionAccessor<T>::max() const
{
    T res = std::numeric_limits<T>::lowest();
    for(index_t i = 0; i < number_of_elements(); i++)
    {
        const T &val = element(i);
        if(val > res)
        {
            res = val;
        }
    }

    return res;
}

//---------------------------------------------------------------------------// 
template <typename T>
T
ExecutionAccessor<T>::sum() const
{
    T res =0;
    for(index_t i = 0; i < number_of_elements(); i++)
    {
        const T &val = element(i);
        res += val;
    }

    return res;
}

//---------------------------------------------------------------------------// 
template <typename T>
float64
ExecutionAccessor<T>::mean() const
{
    float64 res =0;
    for(index_t i = 0; i < number_of_elements(); i++)
    {
        const T &val = element(i);
        res += val;
    }

    res = res / float64(number_of_elements());
    return res;
}

//---------------------------------------------------------------------------// 
template <typename T>
index_t
ExecutionAccessor<T>::count(T val) const
{
    index_t res= 0;
    for(index_t i = 0; i < number_of_elements(); i++)
    {
        if(element(i) == val)
        {
            res++;
        }
    }
    return res;
}

//---------------------------------------------------------------------------//
template <typename T> 
ExecutionAccessor<T> &
ExecutionAccessor<T>::operator=(const ExecutionAccessor<T> &accessor)
{
    if(this != &accessor)
    {
        m_node_ptr = accessor.m_node_ptr;
        m_other_ptr = accessor.m_other_ptr;
        m_other_dtype = accessor.m_other_dtype;
        m_do_i_own_it = accessor.m_do_i_own_it;
        m_data = accessor.m_data;
        m_offset = accessor.m_offset;
        m_stride = accessor.m_stride;
    }
    return *this;
}

//---------------------------------------------------------------------------//
template <typename T> 
T
ExecutionAccessor<T>::element(index_t idx) const
{
    switch(dtype().id())
    {
        // ints
        case DataType::INT8_ID:
            return (T)(*(int8*)(element_ptr(idx)));
        case DataType::INT16_ID: 
            return (T)(*(int16*)(element_ptr(idx)));
        case DataType::INT32_ID:
            return (T)(*(int32*)(element_ptr(idx)));
        case DataType::INT64_ID:
            return (T)(*(int64*)(element_ptr(idx)));
        // uints
        case DataType::UINT8_ID:
            return (T)(*(uint8*)(element_ptr(idx)));
        case DataType::UINT16_ID:
            return (T)(*(uint16*)(element_ptr(idx)));
        case DataType::UINT32_ID:
            return (T)(*(uint32*)(element_ptr(idx)));
        case DataType::UINT64_ID:
            return (T)(*(uint64*)(element_ptr(idx)));
        // floats 
        case DataType::FLOAT32_ID:
            return (T)(*(float32*)(element_ptr(idx)));
        case DataType::FLOAT64_ID:
            return (T)(*(float64*)(element_ptr(idx)));
    }

    // error
    CONDUIT_ERROR("ExecutionAccessor does not support dtype: "
                  << dtype().name());
    return (T)0;
}

//---------------------------------------------------------------------------//
template <typename T>
void
ExecutionAccessor<T>::set(index_t idx, T value)
{
    switch(dtype().id())
    {
        // ints
        case DataType::INT8_ID:
        {
            (*(int8*)(element_ptr(idx))) = static_cast<int8>(value);
            break;
        }
        case DataType::INT16_ID:
        {
            (*(int16*)(element_ptr(idx))) = static_cast<int16>(value);
            break;
        }
        case DataType::INT32_ID:
        {
            (*(int32*)(element_ptr(idx))) = static_cast<int32>(value);
            break;
        }
        case DataType::INT64_ID:
        {
            (*(int64*)(element_ptr(idx))) = static_cast<int64>(value);
            break;
        }
        // uints
        case DataType::UINT8_ID:
        {
            (*(uint8*)(element_ptr(idx))) = static_cast<uint8>(value);
            break;
        }
        case DataType::UINT16_ID:
        {
            (*(uint16*)(element_ptr(idx))) = static_cast<uint16>(value);
            break;
        }
        case DataType::UINT32_ID:
        {
            (*(uint32*)(element_ptr(idx))) = static_cast<uint32>(value);
            break;
        }
        case DataType::UINT64_ID:
        {
            (*(uint64*)(element_ptr(idx))) = static_cast<uint64>(value);
            break;
        }
        // floats
        case DataType::FLOAT32_ID:
        {
            (*(float32*)(element_ptr(idx))) = static_cast<float32>(value);
            break;
        }
        case DataType::FLOAT64_ID:
        {
            (*(float64*)(element_ptr(idx))) = static_cast<float64>(value);
            break;
        }
        default:
            // error
            CONDUIT_ERROR("ExecutionAccessor does not support dtype: "
                          << dtype().name());
    }
}

//---------------------------------------------------------------------------//
template <typename T>
void
ExecutionAccessor<T>::fill(T value)
{
    switch(dtype().id())
    {
        // ints
        case DataType::INT8_ID:
        {
            int8 v = static_cast<int8>(value);
            for(index_t i=0;i < dtype().number_of_elements(); i++)
            {
                 (*(int8*)(element_ptr(i))) = v;
            }
            break;
        }
        case DataType::INT16_ID:
        {
            int16 v = static_cast<int16>(value);
            for(index_t i=0;i < dtype().number_of_elements(); i++)
            {
                 (*(int16*)(element_ptr(i))) = v;
            }
            break;
        }
        case DataType::INT32_ID:
        {
            int32 v = static_cast<int32>(value);
            for(index_t i=0;i < dtype().number_of_elements(); i++)
            {
                 (*(int32*)(element_ptr(i))) = v;
            }
            break;
        }
        case DataType::INT64_ID:
        {
            int64 v = static_cast<int64>(value);
            for(index_t i=0;i < dtype().number_of_elements(); i++)
            {
                 (*(int64*)(element_ptr(i))) = v;
            }
            break;
        }
        // uints
        case DataType::UINT8_ID:
        {
            uint8 v = static_cast<uint8>(value);
            for(index_t i=0;i < dtype().number_of_elements(); i++)
            {
                 (*(uint8*)(element_ptr(i))) = v;
            }
            break;
        }
        case DataType::UINT16_ID:
        {
            uint16 v = static_cast<uint16>(value);
            for(index_t i=0;i < dtype().number_of_elements(); i++)
            {
                 (*(uint16*)(element_ptr(i))) = v;
            }
            break;
        }
        case DataType::UINT32_ID:
        {
            uint32 v = static_cast<uint32>(value);
            for(index_t i=0;i < dtype().number_of_elements(); i++)
            {
                 (*(uint32*)(element_ptr(i))) = v;
            }
            break;
        }
        case DataType::UINT64_ID:
        {
            uint64 v = static_cast<uint64>(value);
            for(index_t i=0;i < dtype().number_of_elements(); i++)
            {
                 (*(uint64*)(element_ptr(i))) = v;
            }
            break;
        }
        // floats
        case DataType::FLOAT32_ID:
        {
            float32 v = static_cast<float32>(value);
            for(index_t i=0;i < dtype().number_of_elements(); i++)
            {
                 (*(float32*)(element_ptr(i))) = v;
            }
            break;
        }
        case DataType::FLOAT64_ID:
        {
            float64 v = static_cast<float64>(value);
            for(index_t i=0;i < dtype().number_of_elements(); i++)
            {
                 (*(float64*)(element_ptr(i))) = v;
            }
            break;
        }
        default:
            // error
            CONDUIT_ERROR("ExecutionAccessor does not support dtype: "
                          << dtype().name());
    }
}

//---------------------------------------------------------------------------//
template <typename T>
void
ExecutionAccessor<T>::use_with(conduit::execution::policy policy)
{
    // we are being asked to execute on the device
    if (policy == conduit::execution::policy::Device)
    {
        // data is already on the device
        if (DeviceMemory::is_device_ptr(m_data))
        {
            // Do nothing
        }
        else // m_data is on the host
        {
            // if we started out on the host
            if (m_node_ptr->data_ptr() == m_data)
            {
                CONDUIT_ASSERT(m_other_ptr == nullptr,
                    "Using accessor in this way will result in a memory leak.");

                // allocate new memory and create a new dtype
                m_other_ptr = DeviceMemory::allocate(dtype().element_bytes() * number_of_elements());
                m_do_i_own_it = true;
                m_other_dtype = DataType(dtype().id(),
                                         number_of_elements(),
                                         0, // offset is 0
                                         DataType::default_bytes(dtype().id()), // stride
                                         dtype().element_bytes(),
                                         dtype().endianness());

                // copy data
                utils::conduit_memcpy_strided_elements(m_other_ptr,
                                                       number_of_elements(),
                                                       dtype().element_bytes(),
                                                       m_other_dtype.stride(),
                                                       m_data,
                                                       dtype().stride());

                // change where our data pointer points and update offset and stride
                m_data = m_other_ptr;
                m_offset = m_other_dtype.offset();
                m_stride = m_other_dtype.stride();
            }
            else // we started out on the device
            {
                CONDUIT_ASSERT(m_data == m_other_ptr, "TODO");

                // call sync to bring our copy of the data on the host back to the device
                sync();

                // dealloc the ptr on the host now that we have copied back
                HostMemory::deallocate(m_data);
                m_do_i_own_it = false;
                m_other_dtype = DataType::empty();

                // set m_data to device data and update offset and stride
                m_data = m_node_ptr->data_ptr();
                // the order of operations is important here; changing the pointer
                // will change the result of calling dtype().
                m_offset = dtype().offset();
                m_stride = dtype().stride();

                // reset m_other_ptr
                m_other_ptr = nullptr;
            }
        }
    }
    // TODO do we support other cases here? Serial, Device, Cuda, Hip, OpenMP
    else // we are being asked to execute on the host
    {
        // data is already on the host
        if (! DeviceMemory::is_device_ptr(m_data))
        {
            // Do nothing
        }
        else // m_data is on the device
        {
            // if we started out on the device
            if (m_node_ptr->data_ptr() == m_data)
            {
                CONDUIT_ASSERT(m_other_ptr == nullptr,
                    "Using accessor in this way will result in a memory leak.");

                // allocate new memory and create a new dtype
                m_other_ptr = HostMemory::allocate(dtype().element_bytes() * number_of_elements());
                m_do_i_own_it = true;
                m_other_dtype = DataType(dtype().id(),
                                         number_of_elements(),
                                         0, // offset is 0
                                         DataType::default_bytes(dtype().id()), // stride
                                         dtype().element_bytes(),
                                         dtype().endianness());

                // copy data
                utils::conduit_memcpy_strided_elements(m_other_ptr,
                                                       number_of_elements(),
                                                       dtype().element_bytes(),
                                                       m_other_dtype.stride(),
                                                       m_data,
                                                       dtype().stride());

                // change where our data pointer points and update offset and stride
                m_data = m_other_ptr;
                m_offset = m_other_dtype.offset();
                m_stride = m_other_dtype.stride();
            }
            else // we started out on the host
            {
                CONDUIT_ASSERT(m_data == m_other_ptr, "TODO");

                // call sync to bring our copy of the data on the device back to the host
                sync();

                // dealloc the ptr on the host now that we have copied back
                DeviceMemory::deallocate(m_data);
                m_do_i_own_it = false;
                m_other_dtype = DataType::empty();

                // set m_data to host data and update offset and stride
                m_data = m_node_ptr->data_ptr();
                m_offset = dtype().offset();
                m_stride = dtype().stride();

                // reset m_other_ptr
                m_other_ptr = nullptr;
            }
        }
    }
}

//---------------------------------------------------------------------------//
template <typename T>
void
ExecutionAccessor<T>::sync()
{
    // if the ptrs don't point to the same place
    if (m_data != m_node_ptr->data_ptr())
    {
        if (!(m_node_ptr->dtype().compatible(dtype()) && 
              number_of_elements() == m_node_ptr->dtype().number_of_elements()))
        {
            m_node_ptr->set(dtype());
        }
        utils::conduit_memcpy_strided_elements(m_node_ptr->data_ptr(),
                                               number_of_elements(),
                                               m_node_ptr->dtype().element_bytes(),
                                               m_node_ptr->dtype().stride(),
                                               m_data,
                                               m_stride);
    }
}


//---------------------------------------------------------------------------//
template <typename T>
void
ExecutionAccessor<T>::assume()
{
    // if the ptrs don't point to the same place
    if (m_data != m_node_ptr->data_ptr())
    {
        CONDUIT_ASSERT(m_data == m_other_ptr, "TODO");

        // reset will deallocate the data the node points to
        m_node_ptr->reset();
        m_node_ptr->schema_ptr()->set(dtype());
        m_node_ptr->set_data_ptr(m_data);

        // we no longer own the data since we have given it to node
        m_other_ptr = nullptr;
        m_do_i_own_it = false;
        m_other_dtype = DataType::empty();
    }
}

//---------------------------------------------------------------------------//
template <typename T>
std::string
ExecutionAccessor<T>::to_string(const std::string &protocol) const
{
    std::ostringstream oss;
    to_string_stream(oss,protocol);
    return oss.str();
}

//---------------------------------------------------------------------------//
template <typename T>
void
ExecutionAccessor<T>::to_string_stream(std::ostream &os,
                                       const std::string &protocol) const
{
    if(protocol == "yaml")
    {
        to_yaml_stream(os);
    }
    else if(protocol == "json")
    {
        to_json_stream(os);
    }
    else
    {
        // unsupported
        CONDUIT_ERROR("Unknown DataType::to_string protocol:" << protocol
                     <<"\nSupported protocols:\n"
                     <<" json, yaml");
    }

}

//---------------------------------------------------------------------------//
template <typename T>
std::string
ExecutionAccessor<T>::to_string_default() const
{
    return to_string();
}

//---------------------------------------------------------------------------//
template <typename T>
std::string
ExecutionAccessor<T>::to_json() const
{
    std::ostringstream oss;
    to_json_stream(oss);
    return oss.str();
}

//---------------------------------------------------------------------------//
template <typename T>
void
ExecutionAccessor<T>::to_json_stream(std::ostream &os) const
{
    index_t nele = number_of_elements();
    // note: nele == 0 case:
    // https://github.com/LLNL/conduit/issues/992
    // we want empty arrays to display as [] not empty string
    if(nele == 0 || nele > 1)
        os << "[";

    bool first=true;
    for(index_t idx = 0; idx < nele; idx++)
    {
        if(!first)
            os << ", ";

        // need to deal with nan and infs for fp cases
        if(std::is_floating_point<T>::value)
        {
            std::string fs = utils::float64_to_string((float64)element(idx));
            //check for inf and nan
            // looking for 'n' covers inf and nan
            bool inf_or_nan = fs.find('n') != std::string::npos;

            if(inf_or_nan)
                os << "\"";

            os << fs;

            if(inf_or_nan)
                os << "\"";
        }
        else
        {
            os << element(idx);
        }

        first=false;
    }
    // note: nele == 0 case:
    // https://github.com/LLNL/conduit/issues/992
    // we want empty arrays to display as [] not empty string
    if(nele == 0 || nele > 1)
        os << "]";
}

//---------------------------------------------------------------------------//
template <typename T>
std::string
ExecutionAccessor<T>::to_yaml() const
{
    std::ostringstream oss;
    to_yaml_stream(oss);
    return oss.str();
}

//---------------------------------------------------------------------------//
template <typename T>
void
ExecutionAccessor<T>::to_yaml_stream(std::ostream &os) const
{
    // yep, its the same as to_json_stream ...
    to_json_stream(os);;
}

//---------------------------------------------------------------------------//
template <typename T>
std::string
ExecutionAccessor<T>::to_summary_string_default() const
{
    return to_summary_string();
}

//---------------------------------------------------------------------------//
template <typename T>
std::string
ExecutionAccessor<T>::to_summary_string(index_t threshold) const
{
    std::ostringstream oss;
    to_summary_string_stream(oss, threshold);
    return oss.str();
}

//---------------------------------------------------------------------------//
template <typename T>
void
ExecutionAccessor<T>::to_summary_string_stream(std::ostream &os,
                                          index_t threshold) const
{
    // if we are less than or equal to threshold, we use to_yaml
    index_t nele = number_of_elements();

    if(nele <= threshold)
    {
        to_yaml_stream(os);
    }
    else
    {
        // if above threshold only show threshold # of values
        index_t half = threshold / 2;
        index_t bottom = half;
        index_t top = half;

        //
        // if odd, show 1/2 +1 first
        //

        if( (threshold % 2) > 0)
        {
            bottom++;
        }

        // note: nele == 0 case:
        // https://github.com/LLNL/conduit/issues/992
        // we want empty arrays to display as [] not empty string
        if(nele == 0 || nele > 1)
            os << "[";

        bool done  = (nele == 0);
        index_t idx = 0;

        while(!done)
        {
            // if not first, add a comma prefix
            if(idx > 0 )
                os << ", ";

            // need to deal with nan and infs for fp cases
            if(std::is_floating_point<T>::value)
            {
                std::string fs = utils::float64_to_string((float64)element(idx));
                //check for inf and nan
                // looking for 'n' covers inf and nan
                bool inf_or_nan = fs.find('n') != std::string::npos;

                if(inf_or_nan)
                    os << "\"";

                os << fs;

                if(inf_or_nan)
                    os << "\"";
            }
            else
            {
                os << element(idx);
            }

            idx++;

            if(idx == bottom)
            {
                idx = nele - top;
                os << ", ...";
            }

            if(idx == nele)
            {
                done = true;
            }
        }

        // note: nele == 0 case:
        // https://github.com/LLNL/conduit/issues/992
        // we want empty arrays to display as [] not empty string
        if(nele == 0 || nele > 1)
            os << "]";

    }
}

//-----------------------------------------------------------------------------
//
// -- conduit::ExecutionAccessor explicit instantiations for supported types --
//
//-----------------------------------------------------------------------------
template class ExecutionAccessor<int8>;
template class ExecutionAccessor<int16>;
template class ExecutionAccessor<int32>;
template class ExecutionAccessor<int64>;

template class ExecutionAccessor<uint8>;
template class ExecutionAccessor<uint16>;
template class ExecutionAccessor<uint32>;
template class ExecutionAccessor<uint64>;

template class ExecutionAccessor<float32>;
template class ExecutionAccessor<float64>;

// gap template instantiations for c-native types

// we never use 'char' directly as a type,
// so we always need to inst the char case
template class ExecutionAccessor<char>;

#ifndef CONDUIT_USE_CHAR
template class ExecutionAccessor<signed char>;
template class ExecutionAccessor<unsigned char>;
#endif

#ifndef CONDUIT_USE_SHORT
template class ExecutionAccessor<signed short>;
template class ExecutionAccessor<unsigned short>;
#endif

#ifndef CONDUIT_USE_INT
template class ExecutionAccessor<signed int>;
template class ExecutionAccessor<unsigned int>;
#endif

#ifndef CONDUIT_USE_LONG
template class ExecutionAccessor<signed long>;
template class ExecutionAccessor<unsigned long>;
#endif

#if defined(CONDUIT_HAS_LONG_LONG) && !defined(CONDUIT_USE_LONG_LONG)
template class ExecutionAccessor<signed long long>;
template class ExecutionAccessor<unsigned long long>;
#endif

#ifndef CONDUIT_USE_FLOAT
template class ExecutionAccessor<float>;
#endif

#ifndef CONDUIT_USE_DOUBLE
template class ExecutionAccessor<double>;
#endif

#ifdef CONDUIT_USE_LONG_DOUBLE
    ltemplate class ExecutionAccessor<long double>;
#endif


}
//-----------------------------------------------------------------------------
// -- end conduit:: --
//-----------------------------------------------------------------------------

