#include <assert.h>
#include <stdbool.h>

#define N 16

void main()
{
  int a[N];
  a[10] = 0;
  bool flag = true;

  for(int i = 0; i < N; ++i)
    // clang-format off
    __CPROVER_assigns(i, __CPROVER_object_whole(a))
    __CPROVER_loop_invariant(
      (0 <= i) && (i <= N) &&
      flag == __CPROVER_forall {
        int k;
        // constant bounds for explicit unrolling with SAT backend
        (0 <= k && k <= N) ==> (
          // the actual symbolic bound for `k`
          k < i ==> a[k] == 1
        )
      }
    )
    // clang-format on
    {
      a[i] = 1;
    }

  assert(a[10] == 1);
}
