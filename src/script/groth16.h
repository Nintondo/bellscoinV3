#ifndef BITCOIN_CRYPTO_GROTH16_H
#define BITCOIN_CRYPTO_GROTH16_H
#include <mcl/bn_c384_256.h>
#include <vector>


#define G16_FP_SIZE_BYTES 48
#define G16_FR_SIZE_BYTES 32

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
    const Groth16VerifierKeyInput *vk,
    Groth16VerifierKeyPrecomputedValues *vkPrecomputed);

int verify_groth16_proof_precomputed(
    const Groth16VerifierKeyInput *vk,
    const Groth16VerifierKeyPrecomputedValues *vkPrecomputed,
    const Groth16ProofInput *proof,
    const mclBnFr *publicInputs);
int deserialize_groth16_vk(Groth16VerifierKeyInput *vk, const char *data, size_t length);
int deserialize_groth16_proof(Groth16ProofInput *vk, mclBnFr *publicInputs, const char *data, size_t length);

/** A verifier class for Groth16 BLS12-381 Zero Knoweledge Proofs. */
class CGROTH16
{
private:
    static bool library_initialized;
public:
    Groth16ProofInput proof;
    Groth16VerifierKeyInput vk;
    Groth16VerifierKeyPrecomputedValues vk_precomputed;
    mclBnFr public_inputs[2];
    CGROTH16()
    {

        if(!CGROTH16::library_initialized){
            CGROTH16::library_initialized = true;
            mclBn_init(MCL_BLS12_381, MCLBN_COMPILED_TIME_VAR);
        }
    };

    int DeserializeVerifierData(const char *data, size_t length);
    int DeserializeProofData(const char *data, size_t length);

    // 6 inputs
    int SetVerifierDataCompact(
        const std::vector<unsigned char> *a,
        const std::vector<unsigned char> *b,
        const std::vector<unsigned char> *c,
        const std::vector<unsigned char> *d,
        const std::vector<unsigned char> *e,
        const std::vector<unsigned char> *f
    );

    // 6 inputs
    int SetProofDataCompact(
        const std::vector<unsigned char> *pi_a,
        const std::vector<unsigned char> *pi_b_0,
        const std::vector<unsigned char> *pi_b_1,
        const std::vector<unsigned char> *pi_c,
        const std::vector<unsigned char> *public_input_0,
        const std::vector<unsigned char> *public_input_1
    );

    bool Verify();

    // CGROTH16& Reset();
};

#endif // BITCOIN_CRYPTO_GROTH16_H
