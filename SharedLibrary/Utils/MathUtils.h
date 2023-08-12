#pragma once

#include <cstdint>
#include <iostream>

// https://stackoverflow.com/a/36522355
namespace cexp
{

    // Small implementation of std::array, needed until constexpr
    // is added to the function 'reference operator[](size_type)'
    template <typename T, std::size_t N>
    struct array {
        T m_data[N];

        using value_type = T;
        using reference = value_type&;
        using const_reference = const value_type&;
        using size_type = std::size_t;

        // This is NOT constexpr in std::array until C++17
        constexpr reference operator[](size_type i) noexcept {
            return m_data[i];
        }

        constexpr const_reference operator[](size_type i) const noexcept {
            return m_data[i];
        }

        constexpr size_type size() const noexcept {
            return N;
        }
    };

}

// Generates CRC-32 table, algorithm based from this link:
// http://www.hackersdelight.org/hdcodetxt/crc.c.txt
constexpr auto gen_crc32_table() {
    constexpr auto num_bytes = 256;
    constexpr auto num_iterations = 8;
    constexpr auto polynomial = 0xEDB88320;

    auto crc32_table = cexp::array<uint32_t, num_bytes>{};

    for (auto byte = 0u; byte < num_bytes; ++byte) {
        auto crc = byte;

        for (auto i = 0; i < num_iterations; ++i) {
            auto mask = -(crc & 1);
            crc = (crc >> 1) ^ (polynomial & mask);
        }

        crc32_table[byte] = crc;
    }

    return crc32_table;
}

// Stores CRC-32 table and softly validates it.
static constexpr auto crc32_table = gen_crc32_table();
static_assert(
    crc32_table.size() == 256 &&
    crc32_table[1] == 0x77073096 &&
    crc32_table[255] == 0x2D02EF8D,
    "gen_crc32_table generated unexpected result."
    );

// Generates CRC-32 code from null-terminated, c-string,
// algorithm based from this link:
// http://www.hackersdelight.org/hdcodetxt/crc.c.txt 
constexpr auto crc32(const char* in) {
    auto crc = 0xFFFFFFFFu;

    for (auto i = 0u; auto c = in[i]; ++i) {
        crc = crc32_table[(crc ^ c) & 0xFF] ^ (crc >> 8);
    }

    return ~crc;
}

// TODO: Dim can be put into template for optimization.
namespace SharedLib
{
    struct HFVec2
    {
        float ele[2];
    };

    template<typename T>
    inline void MatrixMul4x4(const T mat1[16], const T mat2[16], T* resMat)
    {
        for (uint32_t row = 0; row < 4; row++)
        {
            for (uint32_t col = 0; col < 4; col++)
            {
                uint32_t idx = 4 * row + col;
                resMat[idx] = mat1[4 * row] * mat2[col] +
                    mat1[4 * row + 1] * mat2[4 + col] +
                    mat1[4 * row + 2] * mat2[8 + col] +
                    mat1[4 * row + 3] * mat2[12 + col];
            }
        }
    }

    template<typename T>
    inline void MatMulMat(T* mat1, T* mat2, T* resMat, uint32_t dim)
    {
        for (uint32_t row = 0; row < dim; row++)
        {
            for (uint32_t col = 0; col < dim; col++)
            {
                uint32_t idx = dim * row + col;
                resMat[idx] = 0;
                for (uint32_t ele = 0; ele < dim; ele++)
                {
                    resMat[idx] += (mat1[dim * row + ele] * mat2[dim * ele + col]);
                }
            }
        }
    }

    template<typename T>
    inline T Norm(T* vec, uint32_t dim)
    {
        T res = 0;
        for (uint32_t i = 0; i < dim; i++)
        {
            res += (vec[i] * vec[i]);
        }
        return sqrt(res);
    }

    template<typename T>
    inline bool NormalizeVec(T* vec, uint32_t dim)
    {
        T l2Norm = Norm(vec, dim);
        if (l2Norm == 0)
        {
            return false;
        }
        else
        {
            for (uint32_t i = 0; i < dim; i++)
            {
                vec[i] = vec[i] / l2Norm;
            }
            return true;
        }
    }

    template<typename T>
    inline void CrossProductVec3(T* vec1, T* vec2, T* resVec)
    {
        resVec[0] = vec1[1] * vec2[2] - vec1[2] * vec2[1];
        resVec[1] = vec1[2] * vec2[0] - vec1[0] * vec2[2];
        resVec[2] = vec1[0] * vec2[1] - vec1[1] * vec2[0];
    }

    template<typename T>
    inline T DotProduct(T* vec1, T* vec2, uint32_t dim)
    {
        T res = 0;
        for (uint32_t i = 0; i < dim; i++)
        {
            res += (vec1[i] * vec2[i]);
        }
        return res;
    }

    template<typename T>
    inline void ScalarMul(T scalar, T* vec, uint32_t dim)
    {
        for (uint32_t i = 0; i < dim; i++)
        {
            vec[i] *= scalar;
        }
    }

    template<typename T>
    inline void MatMulVec(const T* mat, T* vec, uint32_t dim, T* res)
    {
        for (uint32_t row = 0; row < dim; row++)
        {
            T ele = 0;
            for (uint32_t col = 0; col < dim; col++)
            {
                ele += (mat[row * dim + col] * vec[col]);
            }
            res[row] = ele;
        }
    }

    template<typename T>
    inline void VecAdd(const T* vec1, const T* vec2, uint32_t dim, T* res)
    {
        for (uint32_t i = 0; i < dim; i++)
        {
            res[i] = vec1[i] + vec2[i];
        }
    }

    // NOTE: All matrix on the host are row-major but all matrix on GLSL are column-major.
    // It means we need to do a matrix transpose before sending a matrix to the device/GLSL.
    template<typename T>
    inline void MatTranspose(T* mat, uint32_t dim)
    {
        for (uint32_t row = 0; row < dim; row++)
        {
            for (uint32_t col = row + 1; col < dim; col++)
            {
                uint32_t rowMajIdx = row * dim + col;
                uint32_t colMajIdx = col * dim + row;

                T rowMajEle = mat[rowMajIdx];
                T colMajEle = mat[colMajIdx];

                mat[colMajIdx] = rowMajEle;
                mat[rowMajIdx] = colMajEle;
            }
        }
    }

    // Generate 4x4 matrices
    // Realtime rendering -- P67
    void GenViewMat(float* const pView, float* const pPos, float* const pWorldUp, float* pResMat);

    // Realtime rendering -- P99. Far are near are posive, which correspond to f' and n'. And far > near.
    void GenPerspectiveProjMat(float near, float far, float fov, float aspect, float* pResMat);

    // Realtime rendering -- P70, P65. E = R (roll -- z) * R (pitch -- x) * R (head -- y)
    void GenModelMat(float* pPos, float roll, float pitch, float head, float* pScale, float* pResMat);

    void GenRotationMat(float roll, float pitch, float head, float* pResMat);

    // Realtime rendering -- P75 -- Eqn(4.30)
    void GenRotationMatArb(float* axis, float radien, float* pResMat);
}