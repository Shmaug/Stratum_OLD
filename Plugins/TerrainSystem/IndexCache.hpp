#pragma once

#include <vector>

int* MakeIndicies(uint32_t index, uint32_t GridSize) {
    bool fanLeft = (index & 1) >= 1;
    bool fanUp = (index & 2) >= 1;
    bool fanRight = (index & 4) >= 1;
    bool fanDown = (index & 8) >= 1;

    std::vector<int> inds;
    uint32_t s = GridSize;
    uint32_t i0, i1, i2, i3, i4, i5, i6, i7, i8;
    for (uint32_t x = 0; x < s - 2; x += 2) {
        for (uint32_t z = 0; z < s - 2; z += 2) {
            i0 = (x + 0) * s + z;
            i1 = (x + 1) * s + z;
            i2 = (x + 2) * s + z;
            
            i3 = (x + 0) * s + z + 1;
            i4 = (x + 1) * s + z + 1;
            i5 = (x + 2) * s + z + 1;
            
            i6 = (x + 0) * s + z + 2;
            i7 = (x + 1) * s + z + 2;
            i8 = (x + 2) * s + z + 2;

            if (fanUp && z == s - 3) {
                if (fanRight && x == s - 3) {
                    #pragma region Fan right/up
                    //    i6 --- i7 --- i8
                    //    |  \        /  |
                    //    |    \    /    |
                    // z+ i3 --- i4     i5
                    //    |  \    | \    |
                    //    |    \  |   \  |
                    //    i0 --- i1 --- i2
                    //           x+
                    inds.push_back({
                            i6, i8, i4,
                            i8, i2, i4,
                            i6, i4, i3,
                            i3, i4, i1,
                            i3, i1, i0,
                            i4, i2, i1
                        });
                    #pragma endregion
                } else if (fanLeft && x == 0) {
                    #pragma region Fan left/up
                    //    i6 --- i7 --- i8
                    //    |  \        /  |
                    //    |    \    /    |
                    // z+ i3     i4 --- i5
                    //    |    /  | \    |
                    //    |  /    |   \  |
                    //    i0 --- i1 --- i2
                    //           x+
                    inds.push_back({
                            i6, i8, i4,
                            i6, i4, i0,
                            i8, i5, i4,
                            i4, i5, i2,
                            i4, i2, i1,
                            i4, i1, i0
                        });
                    #pragma endregion
                } else {
                    #pragma region Fan up
                    //    i6 --- i7 --- i8
                    //    |  \        /  |
                    //    |    \    /    |
                    // z+ i3 --- i4 --- i5
                    //    |  \    | \    |
                    //    |    \  |   \  |
                    //    i0 --- i1 --- i2
                    //           x+
                    inds.push_back({
                            i6, i4, i3,
                            i6, i8, i4,
                            i4, i8, i5,

                            i3, i4, i1,
                            i3, i1, i0,
                            i4, i5, i2,
                            i4, i2, i1
                        });
                    #pragma endregion
                }
            } else if (fanDown && z == 0) {
                if (fanRight && x == s - 3) {
                    #pragma region Fan right/down
                    //    i6 --- i7 --- i8
                    //    |  \    |   /  |
                    //    |    \  | /    |
                    // z+ i3 --- i4     i5
                    //    |    /    \    |
                    //    |  /        \  |
                    //    i0 --- i1 --- i2
                    //           x+
                    inds.push_back({
                            i6, i7, i4,
                            i6, i4, i3,
                            i3, i4, i0,
                            i0, i4, i2,
                            i7, i8 ,i4,
                            i4, i8, i2
                        });
                    #pragma endregion
                } else if (fanLeft && x == 0) {
                    #pragma region Fan left/down
                    //    i6 --- i7 --- i8
                    //    |  \    | \    |
                    //    |    \  |   \  |
                    // z+ i3     i4 --- i5
                    //    |    /    \    |
                    //    |  /        \  |
                    //    i0 --- i1 --- i2
                    //           x+
                    inds.push_back({
                            i6, i7, i4,
                            i7, i8, i5,
                            i7, i5, i4,
                            i4, i5, i2,
                            i6, i4, i0,
                            i0, i4, i2
                        });
                    #pragma endregion
                } else {
                    #pragma region Fan down
                    //    i6 --- i7 --- i8
                    //    |  \    | \    |
                    //    |    \  |   \  |
                    // z+ i3 --- i4 --- i5
                    //    |    /    \    |
                    //    |  /        \  |
                    //    i0 --- i1 --- i2
                    //           x+
                    inds.push_back({
                            i6, i7, i4,
                            i6, i4, i3,
                            i7, i8, i5,
                            i7, i5, i4,

                            i3, i4, i0,
                            i0, i4, i2,
                            i4, i5, i2
                        });
                    #pragma endregion
                }
            } else if (fanRight && x == s - 3) {
                #pragma region Fan right
                //    i6 --- i7 --- i8
                //    |  \    |   /  |
                //    |    \  | /    |
                // z+ i3 --- i4     i5
                //    |  \    | \    |
                //    |    \  |   \  |
                //    i0 --- i1 --- i2
                //           x+
                inds.push_back({
                    i6, i7, i4,
                    i6, i4, i3,
                    i3, i4, i1,
                    i3, i1, i0,

                    i7, i8, i4,
                    i8, i2, i4,
                    i4, i2, i1
                });
                #pragma endregion
            } else if (fanLeft && x == 0) {
                #pragma region Fan left
                //    i6 --- i7 --- i8
                //    |  \    | \    |
                //    |    \  |   \  |
                // z+ i3     i4 --- i5
                //    |    /  | \    |
                //    |  /    |   \  |
                //    i0 --- i1 --- i2
                //           x+
                inds.push_back({
                    i6, i7, i4,
                    i6, i4, i0,
                    i7, i8, i5,
                    i7, i5, i4,
                    i4, i5, i2,
                    i4, i2, i1,
                    i4, i1, i0
                });
                #pragma endregion
            } else {
                #pragma region No fan
                //    i6 --- i7 --- i8
                //    |  \    | \    |
                //    |    \  |   \  |
                // z+ i3 --- i4 --- i5
                //    |  \    | \    |
                //    |    \  |   \  |
                //    i0 --- i1 --- i2
                //           x+
                inds.push_back({
                    i6, i7, i4,
                    i6, i4, i3,
                    i7, i8, i5,
                    i7, i5, i4,
                    i3, i4, i1,
                    i3, i1, i0,
                    i4, i5, i2,
                    i4, i2, i1
                });
                #pragma endregion
            }
        }
    }
    return inds;
}