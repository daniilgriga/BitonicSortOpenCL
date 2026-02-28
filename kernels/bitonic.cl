__kernel void bitonic_sort_step (__global int* data, const int j, const int k)
{
    const uint i = get_global_id(0);
    const uint ixj = i ^ (uint)j;

    if (ixj > i)
    {
        const int a = data[i];
        const int b = data[ixj];

        const int ascending = ((i & (uint)k) == 0);

        if ((ascending && a > b) || (!ascending && a < b))
        {
            data[i] = b;
            data[ixj] = a;
        }
    }
}