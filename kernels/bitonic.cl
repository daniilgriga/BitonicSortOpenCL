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
