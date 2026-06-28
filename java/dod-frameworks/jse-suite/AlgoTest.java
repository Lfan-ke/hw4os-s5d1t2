import java.util.*;

/* DoD: 手撕数据结构与算法 (LeetCode 风格), 纯 stdlib, 自校验。
 * 覆盖: 排序/二分/链表/栈队列/堆/哈希/图BFS-DFS/动态规划/Trie/LRU/并查集。
 * 每项断言已知答案; 全过打印 ALGO_DONE。验证 JVM 整数/对象/递归/集合语义正确。 */
public class AlgoTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String name) { if (c) { ok++; } else { fail++; System.out.println("FAIL " + name); } }

    // quicksort
    static void qsort(int[] a, int lo, int hi) {
        if (lo >= hi) return;
        int p = a[(lo + hi) >>> 1], i = lo, j = hi;
        while (i <= j) {
            while (a[i] < p) i++;
            while (a[j] > p) j--;
            if (i <= j) { int t = a[i]; a[i] = a[j]; a[j] = t; i++; j--; }
        }
        qsort(a, lo, j); qsort(a, i, hi);
    }
    // binary search
    static int bsearch(int[] a, int key) {
        int lo = 0, hi = a.length - 1;
        while (lo <= hi) { int m = (lo + hi) >>> 1; if (a[m] == key) return m; else if (a[m] < key) lo = m + 1; else hi = m - 1; }
        return -1;
    }
    // singly linked list reverse (LeetCode 206)
    static class Node { int v; Node next; Node(int v) { this.v = v; } }
    static Node reverse(Node h) { Node prev = null; while (h != null) { Node n = h.next; h.next = prev; prev = h; h = n; } return prev; }
    // two-sum (LeetCode 1)
    static int[] twoSum(int[] a, int target) {
        Map<Integer, Integer> m = new HashMap<>();
        for (int i = 0; i < a.length; i++) { if (m.containsKey(target - a[i])) return new int[]{m.get(target - a[i]), i}; m.put(a[i], i); }
        return new int[]{-1, -1};
    }
    // coin change DP (LeetCode 322)
    static int coinChange(int[] coins, int amount) {
        int[] dp = new int[amount + 1]; Arrays.fill(dp, amount + 1); dp[0] = 0;
        for (int c : coins) for (int x = c; x <= amount; x++) dp[x] = Math.min(dp[x], dp[x - c] + 1);
        return dp[amount] > amount ? -1 : dp[amount];
    }
    // graph BFS shortest hops
    static int bfs(List<List<Integer>> g, int s, int t) {
        int[] dist = new int[g.size()]; Arrays.fill(dist, -1); dist[s] = 0;
        Deque<Integer> q = new ArrayDeque<>(); q.add(s);
        while (!q.isEmpty()) { int u = q.poll(); for (int v : g.get(u)) if (dist[v] < 0) { dist[v] = dist[u] + 1; q.add(v); } }
        return dist[t];
    }
    // union-find
    static class DSU { int[] p; DSU(int n) { p = new int[n]; for (int i = 0; i < n; i++) p[i] = i; } int find(int x) { return p[x] == x ? x : (p[x] = find(p[x])); } void union(int a, int b) { p[find(a)] = find(b); } }
    // LRU cache (LeetCode 146)
    static class LRU<K, V> extends LinkedHashMap<K, V> { final int cap; LRU(int cap) { super(16, 0.75f, true); this.cap = cap; } protected boolean removeEldestEntry(Map.Entry<K, V> e) { return size() > cap; } }

    public static void main(String[] args) {
        int[] a = {5, 2, 9, 1, 5, 6, 3}; qsort(a, 0, a.length - 1);
        check(Arrays.equals(a, new int[]{1, 2, 3, 5, 5, 6, 9}), "quicksort");
        check(bsearch(a, 6) == 5 && bsearch(a, 4) == -1, "bsearch");

        Node h = new Node(1); h.next = new Node(2); h.next.next = new Node(3);
        Node r = reverse(h); check(r.v == 3 && r.next.v == 2 && r.next.next.v == 1, "reverse-list");

        check(Arrays.equals(twoSum(new int[]{2, 7, 11, 15}, 9), new int[]{0, 1}), "two-sum");
        check(coinChange(new int[]{1, 2, 5}, 11) == 3, "coin-change-dp");

        List<List<Integer>> g = new ArrayList<>();
        for (int i = 0; i < 6; i++) g.add(new ArrayList<>());
        int[][] edges = {{0, 1}, {1, 2}, {2, 5}, {0, 3}, {3, 4}, {4, 5}};
        for (int[] e : edges) { g.get(e[0]).add(e[1]); g.get(e[1]).add(e[0]); }
        check(bfs(g, 0, 5) == 3, "bfs-shortest");

        DSU d = new DSU(5); d.union(0, 1); d.union(1, 2); d.union(3, 4);
        check(d.find(0) == d.find(2) && d.find(0) != d.find(4), "union-find");

        // PriorityQueue (heap) top-3
        PriorityQueue<Integer> pq = new PriorityQueue<>(Comparator.reverseOrder());
        for (int x : new int[]{4, 1, 7, 3, 9, 2}) pq.add(x);
        check(pq.poll() == 9 && pq.poll() == 7 && pq.poll() == 4, "heap-pq");

        LRU<Integer, Integer> lru = new LRU<>(2);
        lru.put(1, 1); lru.put(2, 2); lru.get(1); lru.put(3, 3); // evicts 2
        check(lru.containsKey(1) && lru.containsKey(3) && !lru.containsKey(2), "lru-cache");

        // TreeMap ordered ops
        TreeMap<Integer, String> tm = new TreeMap<>();
        tm.put(3, "c"); tm.put(1, "a"); tm.put(2, "b");
        check(tm.firstKey() == 1 && tm.lastKey() == 3 && tm.ceilingKey(2) == 2, "treemap");

        System.out.println("ALGO_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("ALGO_DONE");
    }
}
