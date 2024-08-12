#ifndef BITCOIN_CRYPTO_GROTH16_H
#define BITCOIN_CRYPTO_GROTH16_H
#include "mcl/bn_c384_256.h"
#include <vector>

// Proof Inputs
typedef struct
{
    mclBnG1 pi_1; // [π₁]₁
    mclBnG2 pi_2; // [π₂]₂
    mclBnG1 pi_3; // [π₃]₁
} Groth16ProofInput;


// Minimal Verifier Key
typedef struct
{
    mclBnG1 alpha; // [α]₁
    mclBnG1 k[3];  // [Kᵥ]₁ (3 => because we have two public inputs)
    mclBnG2 beta;  // [β]₂
    mclBnG2 delta; // [δ]₂
    mclBnG2 gamma; // [γ]₂
} Groth16VerifierKeyInput;

// Verifier Key Precomputed Values
typedef struct
{
    mclBnG2 deltaNeg;   // -[δ]₂
    mclBnG2 gammaNeg;   // -[γ]₂
    mclBnGT eAlphaBeta; // e(α, β)
} Groth16VerifierKeyPrecomputedValues;

void precompute_groth16_values(
    const Groth16VerifierKeyInput* vk,
    Groth16VerifierKeyPrecomputedValues* vkPrecomputed);

/** A verifier class for Groth16 BLS12-381 Zero Knoweledge Proofs. */
class CGROTH16
{
private:
    Groth16ProofInput proof;
    Groth16VerifierKeyInput vk;
    Groth16VerifierKeyPrecomputedValues vk_precomputed;
    mclBnFr public_inputs[2];

public:
    CGROTH16();

    bool SetPi1(
        const std::vector<unsigned char>* x,
        const std::vector<unsigned char>* y
    );

    bool SetPi2(
        const std::vector<unsigned char>* x_a0,
        const std::vector<unsigned char>* x_a1,
        const std::vector<unsigned char>* y_a0,
        const std::vector<unsigned char>* y_a1
    );

    bool SetPi3(
        const std::vector<unsigned char>* x,
        const std::vector<unsigned char>* y
    );


    bool SetPublicInputs(
        const std::vector<unsigned char>* public_input_0,
        const std::vector<unsigned char>* public_input_1
    );

    bool SetAlpha(
        const std::vector<unsigned char>* x,
        const std::vector<unsigned char>* y
    );

    bool SetBeta(
        const std::vector<unsigned char>* x_a0,
        const std::vector<unsigned char>* x_a1,
        const std::vector<unsigned char>* y_a0,
        const std::vector<unsigned char>* y_a1
    );

    bool SetDelta(
        const std::vector<unsigned char>* x_a0,
        const std::vector<unsigned char>* x_a1,
        const std::vector<unsigned char>* y_a0,
        const std::vector<unsigned char>* y_a1
    );

    bool SetGamma(
        const std::vector<unsigned char>* x_a0,
        const std::vector<unsigned char>* x_a1,
        const std::vector<unsigned char>* y_a0,
        const std::vector<unsigned char>* y_a1
    );

    bool SetK0(
        const std::vector<unsigned char>* x,
        const std::vector<unsigned char>* y
    );
    bool SetK1(
        const std::vector<unsigned char>* x,
        const std::vector<unsigned char>* y
    );
    bool SetK2(
        const std::vector<unsigned char>* x,
        const std::vector<unsigned char>* y
    );


    bool Verify();

    // CGROTH16& Reset();
};

#endif // BITCOIN_CRYPTO_GROTH16_H