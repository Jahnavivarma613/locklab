#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>

/*
 * Educational ABA demonstration.
 *
 * We model pointer identity with integer IDs so the ABA sequence is
 * deterministic and does not dereference freed memory.
 */

typedef struct {
    int pointer_id;
    unsigned version;
} tagged_head_t;

int main(void)
{
    /* Pointer-only CAS: A -> B -> A appears unchanged. */
    atomic_int plain_head;
    atomic_init(&plain_head, 1); /* A */

    int t1_expected = 1;          /* T1 observed A */
    int t1_stale_next = 2;        /* T1 observed A->next as B */

    /* T2 changes A -> B -> A. */
    atomic_store(&plain_head, 2); /* B */
    atomic_store(&plain_head, 1); /* A again */

    bool plain_cas = atomic_compare_exchange_strong(
        &plain_head, &t1_expected, t1_stale_next);

    printf("Pointer-only ABA demonstration\n");
    printf("T1 originally observed head=A, next=B\n");
    printf("T2 changed head A -> B -> A\n");
    printf("T1 CAS result: %s\n", plain_cas ? "SUCCEEDED (ABA)" : "FAILED");
    printf("Head after stale CAS: %s\n\n",
           atomic_load(&plain_head) == 2 ? "B" : "A");

    /*
     * Tagged model: compare both pointer ID and version.
     * The version changes for every head update.
     */
    atomic_uint_fast64_t tagged_state;
    const unsigned long long A_v1 = (1ULL << 32) | 1ULL;
    const unsigned long long B_v2 = (2ULL << 32) | 2ULL;
    const unsigned long long A_v3 = (3ULL << 32) | 1ULL;

    atomic_init(&tagged_state, A_v1);

    unsigned long long tagged_expected = A_v1;

    atomic_store(&tagged_state, B_v2);
    atomic_store(&tagged_state, A_v3);

    bool tagged_cas = atomic_compare_exchange_strong(
        &tagged_state, &tagged_expected, B_v2);

    printf("Version-tagged ABA demonstration\n");
    printf("T1 originally observed (A, version=1)\n");
    printf("T2 changed (A,1) -> (B,2) -> (A,3)\n");
    printf("T1 CAS result: %s\n",
           tagged_cas ? "SUCCEEDED" : "FAILED (ABA DETECTED)");

    if (!tagged_cas)
        puts("PASS: version mismatch rejected T1's stale CAS");
    else
        puts("NOTE: tagged CAS unexpectedly succeeded");

    return tagged_cas ? 1 : 0;
}
