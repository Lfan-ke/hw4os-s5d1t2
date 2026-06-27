/* S10a · 用户态排序程序（快速排序），验证工具链与 ulib。
 * 学生实现 Lomuto 划分 partition()；quicksort/驱动给定。 */
#include "app.h"

static void iswap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

/* Lomuto 划分：以 a[hi] 为枢轴，把所有 < 枢轴的元素挪到左段，
 * 返回枢轴落定后的下标 p，使 a[lo..p-1] < a[p] <= a[p+1..hi]。 */
static int partition(int *a, int lo, int hi) {
    int pivot = a[hi];
    int i = lo;
    for (int j = lo; j < hi; j++) {
        if (a[j] < pivot) { iswap(&a[i], &a[j]); i++; }
    }
    iswap(&a[i], &a[hi]); /* 枢轴归位 */
    return i;
}

static void quicksort(int *a, int lo, int hi) {
    if (lo >= hi) return;
    int p = partition(a, lo, hi);
    quicksort(a, lo, p - 1);
    quicksort(a, p + 1, hi);
}

int app_sort(void) {
    int a[] = { 5, 2, 9, 1, 5, 6, 3, 8, 7, 0, 4, -3, 100, -50, 42, 11, -1 };
    int n = (int)(sizeof(a) / sizeof(a[0]));

    quicksort(a, 0, n - 1);

    /* 自检：必须非降序。 */
    for (int i = 1; i < n; i++) {
        if (a[i - 1] > a[i]) {
            u_puts("[sort] order broken\n");
            return 0;
        }
    }

    u_puts("[sort] sorted:");
    for (int i = 0; i < n; i++) { u_puts(" "); u_putint(a[i]); }
    u_puts("\n");
    u_puts("SORT_PASS\n");
    return 1;
}
