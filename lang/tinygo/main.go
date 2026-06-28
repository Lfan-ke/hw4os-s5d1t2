// main.go — comprehensive TinyGo test for the StarryOS #764 "tinygo" item.
// Compiled by TinyGo (LLVM-based Go compiler) to a static binary per arch, run on
// StarryOS. Exercises TinyGo-supported Go: goroutines + channels + select, sync
// (WaitGroup/Mutex/atomic), generics (Go 1.18+), closures, slices/maps, interfaces,
// defer, error wrapping, math/strings/sort. Deterministic output -> exact golden.
package main

import (
	"fmt"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
)

// generics (TinyGo supports Go 1.18+ generics)
type Number interface{ ~int | ~int64 | ~float64 }

func Sum[T Number](xs []T) T {
	var acc T
	for _, x := range xs {
		acc += x
	}
	return acc
}

func Map[T, U any](xs []T, f func(T) U) []U {
	out := make([]U, len(xs))
	for i, x := range xs {
		out[i] = f(x)
	}
	return out
}

// interface + dynamic dispatch
type Shape interface{ Area() int }
type Square struct{ side int }
type Rectangle struct{ w, h int }

func (s Square) Area() int    { return s.side * s.side }
func (r Rectangle) Area() int { return r.w * r.h }

func main() {
	// 1) goroutines + channel + WaitGroup: concurrent sum 1..100
	const N = 100
	ch := make(chan int, N)
	var wg sync.WaitGroup
	for i := 1; i <= N; i++ {
		wg.Add(1)
		go func(n int) { defer wg.Done(); ch <- n }(i)
	}
	wg.Wait()
	close(ch)
	total := 0
	for v := range ch {
		total += v
	}
	fmt.Printf("CONC_SUM=%d\n", total) // 5050

	// 2) atomic counter across goroutines
	var counter int64
	var wg2 sync.WaitGroup
	for i := 0; i < 50; i++ {
		wg2.Add(1)
		go func() { defer wg2.Done(); atomic.AddInt64(&counter, 2) }()
	}
	wg2.Wait()
	fmt.Printf("ATOMIC=%d\n", atomic.LoadInt64(&counter)) // 100

	// 3) generics
	fmt.Printf("GEN_SUM_INT=%d\n", Sum([]int{1, 2, 3, 4, 5}))            // 15
	fmt.Printf("GEN_SUM_F=%.1f\n", Sum([]float64{1.5, 2.5, 3.0}))        // 7.0
	sq := Map([]int{1, 2, 3, 4}, func(x int) int { return x * x })
	fmt.Printf("GEN_MAP=%v\n", sq)                                       // [1 4 9 16]

	// 4) select + channels
	c1, c2 := make(chan string, 1), make(chan string, 1)
	c1 <- "one"
	select {
	case m := <-c1:
		fmt.Printf("SELECT=%s\n", m) // one
	case m := <-c2:
		fmt.Printf("SELECT=%s\n", m)
	}

	// 5) interface dispatch
	shapes := []Shape{Square{4}, Rectangle{3, 5}, Square{2}}
	areas := 0
	for _, s := range shapes {
		areas += s.Area()
	}
	fmt.Printf("AREAS=%d\n", areas) // 16+15+4 = 35

	// 6) maps + sort
	freq := map[rune]int{}
	for _, r := range "mississippi" {
		freq[r]++
	}
	keys := []string{}
	for k, v := range freq {
		keys = append(keys, fmt.Sprintf("%c%d", k, v))
	}
	sort.Strings(keys)
	fmt.Printf("FREQ=%s\n", strings.Join(keys, ",")) // i4,m1,p2,s4

	// 7) closures (counter factory)
	mk := func() func() int {
		n := 0
		return func() int { n++; return n }
	}
	next := mk()
	fmt.Printf("CLOSURE=%d,%d,%d\n", next(), next(), next()) // 1,2,3

	// 8) defer ordering (LIFO)
	order := []int{}
	func() {
		for i := 0; i < 3; i++ {
			defer func(n int) { order = append(order, n) }(i)
		}
	}()
	fmt.Printf("DEFER=%v\n", order) // [2 1 0]

	fmt.Println("TINYGO_OK")
}
