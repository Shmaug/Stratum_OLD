#pragma once

#include <vector>

void GenerateTriangles(uint8_t fanMask, uint16_t gridSize, std::vector<uint16_t>& inds) {
    bool fanLeft  = fanMask & 1;
    bool fanUp    = fanMask & 2;
	bool fanRight = fanMask & 4;
    bool fanDown  = fanMask & 8;

    uint16_t s = gridSize + 1;

    uint16_t i0, i1, i2, i3, i4, i5, i6, i7, i8;
    for (uint16_t x = 0; x < s - 2; x += 2) {
        for (uint16_t z = 0; z < s - 2; z += 2) {
            i0 = (z + 0) * s + (x + 0);
            i1 = (z + 0) * s + (x + 1);
            i2 = (z + 0) * s + (x + 2);
            
            i3 = (z + 1) * s + (x + 0);
            i4 = (z + 1) * s + (x + 1);
            i5 = (z + 1) * s + (x + 2);
            
            i6 = (z + 2) * s + (x + 0);
            i7 = (z + 2) * s + (x + 1);
            i8 = (z + 2) * s + (x + 2);

            if (fanDown && z == 0) {
                // i3     i4     i5
                //      /    \    
                //    /        \  
                // i0 --- i1 --- i2
                inds.push_back(i4); inds.push_back(i0); inds.push_back(i2);
            } else {
                // i3     i4     i5
                //      / |  \    
                //    /   |    \  
                // i0 --- i1 --- i2
                inds.push_back(i4); inds.push_back(i0); inds.push_back(i1);
                inds.push_back(i2); inds.push_back(i4); inds.push_back(i1);
            }

            if (fanLeft && x == 0) {
                // i6     i7
                // |  \     
                // |    \   
                // i3     i4
                // |    /   
                // |  /     
                // i0     i1
                inds.push_back(i4); inds.push_back(i6); inds.push_back(i0);
            }else{
                // i6     i7
                // |  \    
                // |    \  
                // i3 --- i
                // |    /  
                // |  /    
                // i0     i1
                inds.push_back(i4); inds.push_back(i6); inds.push_back(i3);
                inds.push_back(i4); inds.push_back(i3); inds.push_back(i0);
            }

            if (fanRight && x == s - 3) {
                // i7     i8
                //     /  |
                //   /    |
                // i4     i5
                //   \    |
                //     \  |
                // i1     i2
                inds.push_back(i8); inds.push_back(i4); inds.push_back(i2);
            } else {
                // i7     i8
                //     /  |
                //   /    |
                // i4 --- i5
                //   \    |
                //     \  |
                // i1     i2
                inds.push_back(i8); inds.push_back(i4); inds.push_back(i5);
                inds.push_back(i5); inds.push_back(i4); inds.push_back(i2);
            }

            if (fanUp && z == s - 3) {
                // i6 --- i7 --- i8
                //    \        /  
                //      \    /    
                // i3     i4     i5
                inds.push_back(i8); inds.push_back(i6); inds.push_back(i4);
            } else {
                // i6 --- i7 --- i8
                //    \   |    /  
                //      \ |  /    
                // i3    i4     i5
                inds.push_back(i7); inds.push_back(i6); inds.push_back(i4);
                inds.push_back(i8); inds.push_back(i7); inds.push_back(i4);
            }
        }
    }
}