#ifndef MEINEKRAFT_NOISE_H
#define MEINEKRAFT_NOISE_H

#include <random>
#include "vector.h"

class Noise {
    int seed = 1;
    std::mt19937 engine;
    std::uniform_real_distribution<> distr;
    std::vector<Vec22> grads; /// Normalized gradients
    std::vector<int> perms;

public:
    Noise(): engine(seed), grads(256), distr(0, 1), perms(256) {
        /// Fill the gradients list with random normalized vectors
        for (int i = 0; i < grads.size(); i++) {
            double x = distr(engine);
            double y = distr(engine);
            grads[i] = Vec22{x, y};
        }

        /// Fill gradient lookup array with random indices to the gradients list
        /// Fill with indices from 0 to perms.size()
        for (int i = 0; i < perms.size(); i++) {
            perms[i] = i;
        }
        /// Randomize the order of the indices
        for (int i = 0; i < perms.size(); i++) {
            int j = (int) distr(engine) & perms.size();
            auto swap = perms[i];
            perms[i] = perms[j];
            perms[j] = swap;
        }
    }

    double perlin(int x, int z, Vec3 chunk_pos, int dimension) {
        /// Skew coordinate inside the chunk; double part + int part + skew part = point coordinate
        double a = z % dimension; // Integer offset inside the chunk
        double b = 1 - std::abs(a / dimension); // Float offset inside the chunk (0, 1)
        double c = chunk_pos.z / dimension; // Integer bounds from the world
        double Y = b + c; // Relative position inside the chunk and the chunk from the world coords perspective

        double f = x % dimension;
        double g = 1 - std::abs(f / dimension);
        double h = chunk_pos.x / dimension;
        double X = g + h;

        /// Grid points from the chunk in the world
        int X0 = (int) (chunk_pos.x / dimension);
        int X1 = (int) (chunk_pos.x + dimension) / dimension;
        int Y0 = (int) chunk_pos.z / dimension;
        int Y1 = (int) (chunk_pos.z + dimension) / dimension;

        /// Gradients using hashed indices from lookup list
        Vec22 x0y0 = grads[perms[(Y0 + perms[X0 % perms.size()]) % perms.size()]];
        Vec22 x1y0 = grads[perms[(Y0 + perms[X1 % perms.size()]) % perms.size()]];
        Vec22 x0y1 = grads[perms[(Y1 + perms[X0 % perms.size()]) % perms.size()]];
        Vec22 x1y1 = grads[perms[(Y1 + perms[X1 % perms.size()]) % perms.size()]];

        /// Contribution of gradient vectors by dot product between relative vectors and gradients
        double v00 = dot(x0y0, Vec22{X0 - X, Y0 - Y});
        double v10 = dot(x1y0, Vec22{X1 - X, Y0 - Y});
        double v01 = dot(x0y1, Vec22{X0 - X, Y1 - Y});
        double v11 = dot(x1y1, Vec22{X1 - X, Y1 - Y});

        /// Bi-cubic interpolation to get the final value
        double Wx = 3*pow(X0 - X, 2) - 2*pow(X0 - X, 3);
        double v0 = v00 - Wx*(v00 - v01);
        double v1 = v10 - Wx*(v10 - v11);

        double Wy = 3*pow(Y0 - Y, 2) - 2*pow(Y0 - Y, 3);
        double v = v0 - Wy*(v0 - v1);

        return v;
    }
};

#endif //MEINEKRAFT_NOISE_H