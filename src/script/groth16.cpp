#include "groth16.h"

#define G16_FP_SIZE_BYTES 48
#define G16_FR_SIZE_BYTES 32

int deserialize_fr(mclBnFr* f, const unsigned char* x, size_t size)
{
    if (size != G16_FR_SIZE_BYTES) {
        return 0;
    }

    mclBnFr_setLittleEndian(f, x, G16_FR_SIZE_BYTES);
    return 1;
}


int deserialize_fp(mclBnFp* f, const unsigned char* x, size_t size)
{
    if (size != G16_FP_SIZE_BYTES) {
        return 0;
    }

    mclBnFp_setLittleEndian(f, x, G16_FP_SIZE_BYTES);
    return 1;
}

int deserialize_g1(mclBnG1* g1, const unsigned char* x, size_t size_x, const unsigned char* y, size_t size_y)
{
    if (size_x != G16_FP_SIZE_BYTES || size_y != G16_FP_SIZE_BYTES) {
        return 0;
    }

    deserialize_fp(&g1->x, x, size_x);
    deserialize_fp(&g1->y, y, size_y);
    return 1;
}

int deserialize_g2(
    mclBnG2* g2,
    const unsigned char* x_a0,
    size_t size_x_a0,
    const unsigned char* x_a1,
    size_t size_x_a1,
    const unsigned char* y_a0,
    size_t size_y_a0,
    const unsigned char* y_a1,
    size_t size_y_a1)
{
    if (
        size_x_a0 != G16_FP_SIZE_BYTES ||
        size_x_a1 != G16_FP_SIZE_BYTES ||
        size_y_a0 != G16_FP_SIZE_BYTES ||
        size_y_a1 != G16_FP_SIZE_BYTES) {
        return 0;
    }

    deserialize_fp(&g2->x.d[0], x_a0, size_x_a0);
    deserialize_fp(&g2->x.d[1], x_a1, size_x_a1);
    deserialize_fp(&g2->y.d[0], y_a0, size_y_a0);
    deserialize_fp(&g2->y.d[1], y_a1, size_y_a1);
    return 1;
}

void precompute_groth16_values(
    const Groth16VerifierKeyInput* vk,
    Groth16VerifierKeyPrecomputedValues* vkPrecomputed)
{
    // pre-compute e(α, β)
    mclBn_pairing(&vkPrecomputed->eAlphaBeta, &vk->alpha, &vk->beta);

    // pre-compute -[δ]₂
    mclBnG2_neg(&vkPrecomputed->deltaNeg, &vk->delta);

    // pre-compute -[γ]₂
    mclBnG2_neg(&vkPrecomputed->gammaNeg, &vk->gamma);
}

int verify_groth16_proof_precomputed(
    const Groth16VerifierKeyInput* vk,
    const Groth16VerifierKeyPrecomputedValues* vkPrecomputed,
    const Groth16ProofInput* proof,
    const mclBnFr* publicInputs)
{
    // [Σᵥ (Kᵥ₊₁ * publicInputs[v])]₁
    mclBnG1 sumKTimesPub = vk->k[0];

    // value to store Kᵥ₊₁ * publicInputs[v]
    mclBnG1 tmpKvTimesPubv;

    // compute K₁ * publicInputs[0]
    mclBnG1_mul(&tmpKvTimesPubv, &vk->k[1], &publicInputs[0]);
    // sumKTimesPub += K₁ * publicInputs[0]
    mclBnG1_add(&sumKTimesPub, &sumKTimesPub, &tmpKvTimesPubv);

    // compute K₂ * publicInputs[1]
    mclBnG1_mul(&tmpKvTimesPubv, &vk->k[2], &publicInputs[1]);
    // sumKTimesPub += K₂ * publicInputs[1]
    mclBnG1_add(&sumKTimesPub, &sumKTimesPub, &tmpKvTimesPubv);

    // compute e([π₁]₁, [π₂]₂)
    mclBnGT ePi1Pi2;
    mclBn_millerLoop(&ePi1Pi2, &proof->pi_1, &proof->pi_2);

    // compute e( [Σᵥ (Kᵥ₊₁ * publicInputs[v])]₁, -[γ]₂ )
    mclBnGT eSumKTimesPubGammaNeg;
    mclBn_millerLoop(&eSumKTimesPubGammaNeg, &sumKTimesPub, &vkPrecomputed->gammaNeg);

    // compute e([π₃]₁, -[δ]₂)
    mclBnGT ePi3DeltaNeg;
    mclBn_millerLoop(&ePi3DeltaNeg, &proof->pi_3, &vkPrecomputed->deltaNeg);

    // compute z = e(α, β) * e( [Σᵥ (Kᵥ₊₁ * publicInputs[v])]₁, -[γ]₂ ) * e([π₃]1, -[δ]₂)
    mclBnGT z;
    mclBnGT_mul(&z, &ePi1Pi2, &eSumKTimesPubGammaNeg);
    mclBnGT_mul(&z, &z, &ePi3DeltaNeg);

    // ensure that z is a unique value in GT
    mclBn_finalExp(&z, &z);

    // if e(α, β) * e( [Σᵥ (Kᵥ₊₁ * publicInputs[v])]₁, -[γ]₂ ) * e([π₃]1, -[δ]₂) == e(α, β) then the proof is valid
    return mclBnGT_isEqual(&z, &vkPrecomputed->eAlphaBeta);
}

int verify_groth16_proof(
    const Groth16VerifierKeyInput* vk,
    const Groth16ProofInput* proof,
    const mclBnFr* publicInputs)
{
    Groth16VerifierKeyPrecomputedValues vkPrecomputed;
    precompute_groth16_values(vk, &vkPrecomputed);
    return verify_groth16_proof_precomputed(vk, &vkPrecomputed, proof, publicInputs);
}


bool CGROTH16::SetAlpha(
    const std::vector<unsigned char>* x,
    const std::vector<unsigned char>* y)
{
    if (x->size() != G16_FP_SIZE_BYTES || y->size() != G16_FP_SIZE_BYTES) {
        return false;
    }

    deserialize_g1(&vk.alpha, x->data(), x->size(), y->data(), y->size());
    return true;
}


bool CGROTH16::SetPi1(
    const std::vector<unsigned char>* x,
    const std::vector<unsigned char>* y)
{
    if (x->size() != G16_FP_SIZE_BYTES || y->size() != G16_FP_SIZE_BYTES) {
        return false;
    }

    deserialize_g1(&proof.pi_1, x->data(), x->size(), y->data(), y->size());
    return true;
}

bool CGROTH16::SetPi2(
    const std::vector<unsigned char>* x_a0,
    const std::vector<unsigned char>* x_a1,
    const std::vector<unsigned char>* y_a0,
    const std::vector<unsigned char>* y_a1)
{
    if (
        x_a0->size() != G16_FP_SIZE_BYTES ||
        x_a1->size() != G16_FP_SIZE_BYTES ||
        y_a0->size() != G16_FP_SIZE_BYTES ||
        y_a1->size() != G16_FP_SIZE_BYTES) {
        return false;
    }

    deserialize_g2(&proof.pi_2, x_a0->data(), x_a0->size(), x_a1->data(), x_a1->size(), y_a0->data(), y_a0->size(), y_a1->data(), y_a1->size());
    return true;
}

bool CGROTH16::SetPi3(
    const std::vector<unsigned char>* x,
    const std::vector<unsigned char>* y)
{
    if (x->size() != G16_FP_SIZE_BYTES || y->size() != G16_FP_SIZE_BYTES) {
        return false;
    }

    deserialize_g1(&proof.pi_3, x->data(), x->size(), y->data(), y->size());
    return true;
}

bool CGROTH16::SetPublicInputs(
    const std::vector<unsigned char>* public_input_0,
    const std::vector<unsigned char>* public_input_1)
{
    if (public_input_0->size() != G16_FR_SIZE_BYTES || public_input_1->size() != G16_FR_SIZE_BYTES) {
        return false;
    }

    deserialize_fr(&public_inputs[0], public_input_0->data(), public_input_0->size());
    deserialize_fr(&public_inputs[1], public_input_1->data(), public_input_1->size());
    return true;
}

bool CGROTH16::SetBeta(
    const std::vector<unsigned char>* x_a0,
    const std::vector<unsigned char>* x_a1,
    const std::vector<unsigned char>* y_a0,
    const std::vector<unsigned char>* y_a1)
{
    if (
        x_a0->size() != G16_FP_SIZE_BYTES ||
        x_a1->size() != G16_FP_SIZE_BYTES ||
        y_a0->size() != G16_FP_SIZE_BYTES ||
        y_a1->size() != G16_FP_SIZE_BYTES) {
        return false;
    }

    deserialize_g2(&vk.beta, x_a0->data(), x_a0->size(), x_a1->data(), x_a1->size(), y_a0->data(), y_a0->size(), y_a1->data(), y_a1->size());
    return true;
}

bool CGROTH16::SetDelta(
    const std::vector<unsigned char>* x_a0,
    const std::vector<unsigned char>* x_a1,
    const std::vector<unsigned char>* y_a0,
    const std::vector<unsigned char>* y_a1)
{
    if (
        x_a0->size() != G16_FP_SIZE_BYTES ||
        x_a1->size() != G16_FP_SIZE_BYTES ||
        y_a0->size() != G16_FP_SIZE_BYTES ||
        y_a1->size() != G16_FP_SIZE_BYTES) {
        return false;
    }

    deserialize_g2(&vk.delta, x_a0->data(), x_a0->size(), x_a1->data(), x_a1->size(), y_a0->data(), y_a0->size(), y_a1->data(), y_a1->size());
    return true;
}

bool CGROTH16::SetGamma(
    const std::vector<unsigned char>* x_a0,
    const std::vector<unsigned char>* x_a1,
    const std::vector<unsigned char>* y_a0,
    const std::vector<unsigned char>* y_a1)
{
    if (
        x_a0->size() != G16_FP_SIZE_BYTES ||
        x_a1->size() != G16_FP_SIZE_BYTES ||
        y_a0->size() != G16_FP_SIZE_BYTES ||
        y_a1->size() != G16_FP_SIZE_BYTES) {
        return false;
    }

    deserialize_g2(&vk.gamma, x_a0->data(), x_a0->size(), x_a1->data(), x_a1->size(), y_a0->data(), y_a0->size(), y_a1->data(), y_a1->size());
    return true;
}

bool CGROTH16::Verify() {
    return verify_groth16_proof(&vk, &proof, public_inputs) == 1;
}