#pragma once

#include <vector>

void GenerateTriangles(uint8_t fanMask, uint16_t gridSize, std::vector<uint16_t>& inds) {
    bool fanLeft = fanMask & 1;
    bool fanUp = fanMask & 2;
	bool fanRight = fanMask & 4;
    bool fanDown = fanMask & 8;

    uint16_t i0, i1, i2, i3, i4, i5, i6, i7, i8;
    for (uint16_t x = 0; x < gridSize - 2; x += 2) {
        for (uint16_t z = 0; z < gridSize - 2; z += 2) {
            i0 = (x + 0) * gridSize + z;
            i1 = (x + 1) * gridSize + z;
            i2 = (x + 2) * gridSize + z;
            
            i3 = (x + 0) * gridSize + z + 1;
            i4 = (x + 1) * gridSize + z + 1;
            i5 = (x + 2) * gridSize + z + 1;
            
            i6 = (x + 0) * gridSize + z + 2;
            i7 = (x + 1) * gridSize + z + 2;
            i8 = (x + 2) * gridSize + z + 2;

            if (fanUp && z == gridSize - 3) {
                if (fanRight && x == gridSize - 3) {
                    #pragma region Fan right/up
                    //    i6 --- i7 --- i8
                    //    |  \        /  |
                    //    |    \    /    |
                    // z+ i3 --- i4     i5
                    //    |  \    | \    |
                    //    |    \  |   \  |
                    //    i0 --- i1 --- i2
                    //           x+
                    inds.push_back(i6); inds.push_back(i8); inds.push_back(i4);
                    inds.push_back(i8); inds.push_back(i2); inds.push_back(i4);
                    inds.push_back(i6); inds.push_back(i4); inds.push_back(i3);
                    inds.push_back(i3); inds.push_back(i4); inds.push_back(i1);
                    inds.push_back(i3); inds.push_back(i1); inds.push_back(i0);
                    inds.push_back(i4); inds.push_back(i2); inds.push_back(i1);
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
                    inds.push_back(i6); inds.push_back(i8); inds.push_back(i4);
                    inds.push_back(i6); inds.push_back(i4); inds.push_back(i0);
                    inds.push_back(i8); inds.push_back(i5); inds.push_back(i4);
                    inds.push_back(i4); inds.push_back(i5); inds.push_back(i2);
                    inds.push_back(i4); inds.push_back(i2); inds.push_back(i1);
                    inds.push_back(i4); inds.push_back(i1); inds.push_back(i0);
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
					inds.push_back(i6); inds.push_back(i4); inds.push_back(i3);
					inds.push_back(i6); inds.push_back(i8); inds.push_back(i4);
					inds.push_back(i4); inds.push_back(i8); inds.push_back(i5);
					inds.push_back(i3); inds.push_back(i4); inds.push_back(i1);
					inds.push_back(i3); inds.push_back(i1); inds.push_back(i0);
					inds.push_back(i4); inds.push_back(i5); inds.push_back(i2);
					inds.push_back(i4); inds.push_back(i2); inds.push_back(i1);
                    #pragma endregion
                }
            } else if (fanDown && z == 0) {
                if (fanRight && x == gridSize - 3) {
                    #pragma region Fan right/down
                    //    i6 --- i7 --- i8
                    //    |  \    |   /  |
                    //    |    \  | /    |
                    // z+ i3 --- i4     i5
                    //    |    /    \    |
                    //    |  /        \  |
                    //    i0 --- i1 --- i2
                    //           x+
                    inds.push_back(i6); inds.push_back(i7); inds.push_back(i4);
                    inds.push_back(i6); inds.push_back(i4); inds.push_back(i3);
                    inds.push_back(i3); inds.push_back(i4); inds.push_back(i0);
                    inds.push_back(i0); inds.push_back(i4); inds.push_back(i2);
                    inds.push_back(i7); inds.push_back(i8); inds.push_back(i4);
                    inds.push_back(i4); inds.push_back(i8); inds.push_back(i2);
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
                    inds.push_back(i6); inds.push_back(i7); inds.push_back(i4);
                    inds.push_back(i7); inds.push_back(i8); inds.push_back(i5);
                    inds.push_back(i7); inds.push_back(i5); inds.push_back(i4);
                    inds.push_back(i4); inds.push_back(i5); inds.push_back(i2);
                    inds.push_back(i6); inds.push_back(i4); inds.push_back(i0);
                    inds.push_back(i0); inds.push_back(i4); inds.push_back(i2);
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
                    inds.push_back(i6); inds.push_back(i7); inds.push_back(i4);
                    inds.push_back(i6); inds.push_back(i4); inds.push_back(i3);
                    inds.push_back(i7); inds.push_back(i8); inds.push_back(i5);
                    inds.push_back(i7); inds.push_back(i5); inds.push_back(i4);
                    inds.push_back(i3); inds.push_back(i4); inds.push_back(i0);
                    inds.push_back(i0); inds.push_back(i4); inds.push_back(i2);
                    inds.push_back(i4); inds.push_back(i5); inds.push_back(i2);
                    #pragma endregion
                }
            } else if (fanRight && x == gridSize - 3) {
                #pragma region Fan right
                //    i6 --- i7 --- i8
                //    |  \    |   /  |
                //    |    \  | /    |
                // z+ i3 --- i4     i5
                //    |  \    | \    |
                //    |    \  |   \  |
                //    i0 --- i1 --- i2
                //           x+
                inds.push_back(i6); inds.push_back(i7); inds.push_back(i4);
                inds.push_back(i6); inds.push_back(i4); inds.push_back(i3);
                inds.push_back(i3); inds.push_back(i4); inds.push_back(i1);
                inds.push_back(i3); inds.push_back(i1); inds.push_back(i0);
                inds.push_back(i7); inds.push_back(i8); inds.push_back(i4);
                inds.push_back(i8); inds.push_back(i2); inds.push_back(i4);
                inds.push_back(i4); inds.push_back(i2); inds.push_back(i1);
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
                inds.push_back(i6); inds.push_back(i7); inds.push_back(i4);
                inds.push_back(i6); inds.push_back(i4); inds.push_back(i0);
                inds.push_back(i7); inds.push_back(i8); inds.push_back(i5);
                inds.push_back(i7); inds.push_back(i5); inds.push_back(i4);
                inds.push_back(i4); inds.push_back(i5); inds.push_back(i2);
                inds.push_back(i4); inds.push_back(i2); inds.push_back(i1);
                inds.push_back(i4); inds.push_back(i1); inds.push_back(i0);
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
                inds.push_back(i6); inds.push_back(i7); inds.push_back(i4);
                inds.push_back(i6); inds.push_back(i4); inds.push_back(i3);
                inds.push_back(i7); inds.push_back(i8); inds.push_back(i5);
                inds.push_back(i7); inds.push_back(i5); inds.push_back(i4);
                inds.push_back(i3); inds.push_back(i4); inds.push_back(i1);
                inds.push_back(i3); inds.push_back(i1); inds.push_back(i0);
                inds.push_back(i4); inds.push_back(i5); inds.push_back(i2);
                inds.push_back(i4); inds.push_back(i2); inds.push_back(i1);
                #pragma endregion
            }
        }
    }
}