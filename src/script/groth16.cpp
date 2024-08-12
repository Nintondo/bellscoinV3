#include "groth16.h"
#include <mcl/bn_c384_256.h>
#include <string.h>
#include <vector>
/*
#include <stdio.h>
#include <string>
#include <list>
#include <iostream>
#include <utility>
*/

bool CGROTH16::library_initialized = false;

void printHexChar(const char *prefix, const char *v, size_t size, const char *suffix)
{
    /*
    printf("%s", prefix);
    for (size_t i = 0; i < size; i++)
    {
        printf("%02x", (unsigned char)(v[i]));
    }
    printf("%s", suffix);
    */
}
int deserialize_groth16_vk(Groth16VerifierKeyInput *vk, const char *data, size_t length)
{
    if (length != (4 * mclBn_getG1ByteSize() + 3 * mclBn_getG2ByteSize()))
        return 0;
    const char *ptr = data;
    size_t tmp = mclBnG1_deserialize(&vk->alpha, ptr, G16_FP_SIZE_BYTES);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    tmp = mclBnG1_deserialize(&vk->k[0], ptr, G16_FP_SIZE_BYTES);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    tmp = mclBnG1_deserialize(&vk->k[1], ptr, G16_FP_SIZE_BYTES);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    tmp = mclBnG1_deserialize(&vk->k[2], ptr, G16_FP_SIZE_BYTES);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    tmp = mclBnG2_deserialize(&vk->beta, ptr, G16_FP_SIZE_BYTES * 2);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    tmp = mclBnG2_deserialize(&vk->delta, ptr, G16_FP_SIZE_BYTES * 2);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    tmp = mclBnG2_deserialize(&vk->gamma, ptr, G16_FP_SIZE_BYTES * 2);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    return ptr - data;
}
int serialize_groth16_vk(const Groth16VerifierKeyInput *vk, char *data)
{
    char *ptr = data;
    size_t tmp = mclBnG1_serialize(data, G16_FP_SIZE_BYTES, &vk->alpha);
    if (tmp == 0)
        return 0;
    data += tmp;
    tmp = mclBnG1_serialize(data, G16_FP_SIZE_BYTES, &vk->k[0]);
    if (tmp == 0)
        return 0;
    data += tmp;
    tmp = mclBnG1_serialize(data, G16_FP_SIZE_BYTES, &vk->k[1]);
    if (tmp == 0)
        return 0;
    data += tmp;
    tmp = mclBnG1_serialize(data, G16_FP_SIZE_BYTES, &vk->k[2]);
    if (tmp == 0)
        return 0;
    data += tmp;
    tmp = mclBnG2_serialize(data, G16_FP_SIZE_BYTES * 2, &vk->beta);
    if (tmp == 0)
        return 0;
    data += tmp;
    tmp = mclBnG2_serialize(data, G16_FP_SIZE_BYTES * 2, &vk->delta);
    if (tmp == 0)
        return 0;
    data += tmp;
    tmp = mclBnG2_serialize(data, G16_FP_SIZE_BYTES * 2, &vk->gamma);
    if (tmp == 0)
        return 0;
    data += tmp;
    return ptr - data;
}
int serialize_groth16_proof(const Groth16ProofInput *vk, const mclBnFr *publicInputs, char *data)
{
    char *ptr = data;
    size_t tmp = mclBnG1_serialize(data, G16_FP_SIZE_BYTES, &vk->pi_1);
    if (tmp == 0)
        return 0;
    data += tmp;
    tmp = mclBnG2_serialize(data, G16_FP_SIZE_BYTES * 2, &vk->pi_2);
    if (tmp == 0)
        return 0;
    data += tmp;
    tmp = mclBnG1_serialize(data, G16_FP_SIZE_BYTES, &vk->pi_3);
    if (tmp == 0)
        return 0;
    data += tmp;
    tmp = mclBnFr_serialize(data, G16_FR_SIZE_BYTES, &publicInputs[0]);
    if (tmp == 0)
        return 0;
    data += tmp;
    tmp = mclBnFr_serialize(data, G16_FR_SIZE_BYTES, &publicInputs[1]);
    if (tmp == 0)
        return 0;
    data += tmp;
    return ptr - data;
}




int deserialize_groth16_proof(Groth16ProofInput *vk, mclBnFr *publicInputs, const char *data, size_t length)
{
    if (length != (mclBn_getG2ByteSize() + 2 * mclBn_getG1ByteSize() + 2 * mclBn_getFrByteSize()))
        return 0;
    const char *ptr = data;
    size_t tmp = mclBnG1_deserialize(&vk->pi_1, ptr, G16_FP_SIZE_BYTES);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    tmp = mclBnG2_deserialize(&vk->pi_2, ptr, G16_FP_SIZE_BYTES * 2);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    tmp = mclBnG1_deserialize(&vk->pi_3, ptr, G16_FP_SIZE_BYTES);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    tmp = mclBnFr_deserialize(&publicInputs[0], ptr, G16_FR_SIZE_BYTES);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    tmp = mclBnFr_deserialize(&publicInputs[1], ptr, G16_FR_SIZE_BYTES);
    if (tmp == 0)
        return 0;
    ptr += tmp;
    return ptr - data;
}

void precompute_groth16_values(
    const Groth16VerifierKeyInput *vk,
    Groth16VerifierKeyPrecomputedValues *vkPrecomputed)
{
    // pre-compute e(α, β)
    mclBn_pairing(&vkPrecomputed->eAlphaBeta, &vk->alpha, &vk->beta);

    // pre-compute -[δ]₂
    mclBnG2_neg(&vkPrecomputed->deltaNeg, &vk->delta);

    // pre-compute -[γ]₂
    mclBnG2_neg(&vkPrecomputed->gammaNeg, &vk->gamma);
}

int verify_groth16_proof_precomputed(
    const Groth16VerifierKeyInput *vk,
    const Groth16VerifierKeyPrecomputedValues *vkPrecomputed,
    const Groth16ProofInput *proof,
    const mclBnFr *publicInputs)
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
    const Groth16VerifierKeyInput *vk,
    const Groth16ProofInput *proof,
    const mclBnFr *publicInputs)
{
    Groth16VerifierKeyPrecomputedValues vkPrecomputed;
    precompute_groth16_values(vk, &vkPrecomputed);
    return verify_groth16_proof_precomputed(vk, &vkPrecomputed, proof, publicInputs);
}

bool CGROTH16::Verify()
{

    char data[480] = {0};
    serialize_groth16_proof(&proof, public_inputs, data);
    printHexChar("proof_data_hex_v: [", data, 256, "]\n");
    serialize_groth16_vk(&vk, data);
    printHexChar("verifier_data_hex_v: [", data, 480, "]\n");
    int result = verify_groth16_proof(&vk, &proof, public_inputs);

    return result == 1;
}

int CGROTH16::DeserializeVerifierData(const char *data, size_t length)
{
    return deserialize_groth16_vk(&vk, data, length);
}

int CGROTH16::DeserializeProofData(const char *data, size_t length)
{
    return deserialize_groth16_proof(&proof, public_inputs, data, length);
}

int CGROTH16::SetVerifierDataCompact(
    const std::vector<unsigned char> *a,
    const std::vector<unsigned char> *b,
    const std::vector<unsigned char> *c,
    const std::vector<unsigned char> *d,
    const std::vector<unsigned char> *e,
    const std::vector<unsigned char> *f
){
    //printf("hi vdata\n");
    //printf("got sizes: a->size(): %lu, b->size(): %lu, c->size(): %lu, d->size(): %lu, e->size(): %lu, f->size(): %lu ",a->size() ,b->size() ,c->size() ,d->size() ,e->size() ,f->size());

    if(a->size() != 80 || b->size() != 80 || c->size() != 80 || d->size() != 80 || e->size() != 80 || f->size() != 80){
        return 0;
    }
    char data[480] = {0};
    memcpy(data, a->data(), 80);
    memcpy(data + 80, b->data(), 80);
    memcpy(data + 160, c->data(), 80);
    memcpy(data + 240, d->data(), 80);
    memcpy(data + 320, e->data(), 80);
    memcpy(data + 400, f->data(), 80);
    printHexChar("verifier_data_hex: ", data, 480, "\n");
    return DeserializeVerifierData(data, 480);
}
int CGROTH16::SetProofDataCompact(
    const std::vector<unsigned char> *pi_a,
    const std::vector<unsigned char> *pi_b_0,
    const std::vector<unsigned char> *pi_b_1,
    const std::vector<unsigned char> *pi_c,
    const std::vector<unsigned char> *public_input_0,
    const std::vector<unsigned char> *public_input_1
){
    //printf("hi proof data\n");
    //printf("size pi_a->size(): %lu pi_b_0->size(): %lu pi_b_1->size(): %lu pi_c->size(): %lu public_input_0->size(): %lu public_input_1->size(): %lu\n", pi_a->size(), pi_b_0->size(), pi_b_1->size(), pi_c->size(), public_input_0->size(),public_input_1->size());


    if(pi_a->size() != G16_FP_SIZE_BYTES || pi_b_0->size() != G16_FP_SIZE_BYTES || pi_b_1->size() != G16_FP_SIZE_BYTES || pi_c->size() != G16_FP_SIZE_BYTES || public_input_0->size() != G16_FR_SIZE_BYTES || public_input_1->size() != G16_FR_SIZE_BYTES){
        //printf("bad size pi_a->size(): %lu pi_b_0->size(): %lu pi_b_1->size(): %lu pi_c->size(): %lu public_input_0->size(): %lu public_input_1->size(): %lu\n", pi_a->size(), pi_b_0->size(), pi_b_1->size(), pi_c->size(), public_input_0->size(),public_input_1->size());
        return 0;
    }
    char data[256] = {0};
    memcpy(data, pi_a->data(), G16_FP_SIZE_BYTES);
    memcpy(data + G16_FP_SIZE_BYTES, pi_b_0->data(), G16_FP_SIZE_BYTES);
    memcpy(data + G16_FP_SIZE_BYTES*2, pi_b_1->data(), G16_FP_SIZE_BYTES);
    memcpy(data + G16_FP_SIZE_BYTES*3, pi_c->data(), G16_FP_SIZE_BYTES);
    memcpy(data + G16_FP_SIZE_BYTES*4, public_input_0->data(), G16_FR_SIZE_BYTES);
    memcpy(data + G16_FP_SIZE_BYTES*4+G16_FR_SIZE_BYTES, public_input_1->data(), G16_FR_SIZE_BYTES);
    printHexChar("proof_data_hex: ", data, 256, "\n");
    return DeserializeProofData(data, 256);

}
