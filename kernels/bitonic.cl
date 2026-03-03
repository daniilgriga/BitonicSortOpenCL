__kernel void bitonic_sort_step_half_ctz (
    __global int* data,
    const int j,
    const int k,
    const int j_ctz)
{
    const uint gid = get_global_id(0);
    const uint uj = (uint)j;
    const uint uk = (uint)k;
    const uint up = (uint)j_ctz;

    const uint low = gid & (uj - 1u);
    const uint high = gid >> up;
    const uint i = (high << (up + 1u)) | low;
    const uint ixj = i ^ uj;

    const int a = data[i];
    const int b = data[ixj];
    const int ascending = ((i & uk) == 0u);

    if ((ascending && a > b) || (!ascending && a < b))
    {
        data[i] = b;
        data[ixj] = a;
    }
}


__kernel void bitonic_sort_local (__global int* data, __local int* shared)
{
    const uint gid = get_global_id(0);
    const uint lid = get_local_id(0);
    const uint group = get_group_id(0);
    const uint group_size = get_local_size(0);

    uint base = group * group_size;

    shared[lid] = data[base + lid];

    barrier(CLK_LOCAL_MEM_FENCE);

    for (uint k = 2; k <= group_size; k <<= 1)
    {
        for (uint j = k >> 1; j > 0; j >>= 1)
        {
            uint ixj = lid ^ j;

            if (ixj > lid)
            {
                int a = shared[lid];
                int b = shared[ixj];

                int ascending = ((gid & k) == 0);
                if ((ascending && a > b) || (!ascending && a < b))
                {
                    shared[lid] = b;
                    shared[ixj] = a;
                }

            }

            barrier(CLK_LOCAL_MEM_FENCE);
        }
    }

    data[base + lid] = shared[lid];
}
