# Memory Pool (Placement New) Benchmark

### 실험 환경
- Object Type: `FNode` (64 bytes)
- Object Count: 3,000,000

### Tested Operations:
- 개별 `new`/`delete` (Heap)
- 단일 버퍼에 `placement new` (Arena)

### 측정 지표:
- `Alloc`: 메모리 할당 시간
- `Write`: 메모리 초기화 시간 (cache line touch)
- `Seq`: 순차 접근 (sequential read)
- `Rand`: 랜덤 접근 (random read)
- `RSS`: 프로세스 메모리 사용량 (KB)

---

## 결과

<img width="947" height="513" alt="image" src="https://github.com/user-attachments/assets/2484eeff-5801-4207-a8a9-94f41ca81fb0" />


| Metric               | Heap (new/delete) | Arena (placement new) |         개선 비율 |
| -------------------- | ----------------: | --------------------: | ------------: |
| **Alloc**            |        0.518918 s |           0.0571868 s | **약 9.1× 빠름** |
| **Write**            |       0.0187652 s |           0.0114237 s | **약 1.6× 빠름** |
| **Seq (Sequential)** |       0.0515483 s |           0.0301345 s | **약 1.7× 빠름** |
| **Rand (Random)**    |        0.256815 s |            0.235242 s | **약 1.1× 빠름** |
| **RSS (Memory)**     |        612,320 KB |            612,320 KB |          ≈ 동일 |

---

## 분석

### 할당 속도 (Alloc)
Heap 방식은 **3백만 번의 `malloc` 호출**로 인해 내부 관리 오버헤드가 큼.  
반면 Arena는 **1회 버퍼 할당 후 placement new로 단순히 포인터를 증가시키는 방식**이므로,  
**할당 시간이 약 9배 이상 빠르다.**

---

### 캐시 적중률 (Seq / Write)
Arena는 객체가 **연속된 메모리**에 배치되어 **CPU prefetcher가 효율적으로 작동**.  
그 결과 **순차 접근(Seq)** 에서 약 **1.7배 성능 향상**,  
**Write** 구간에서도 캐시 miss 감소로 **미세한 개선** 확인.

---

### 랜덤 접근 (Rand)
랜덤 접근은 메모리 지역성이 깨지므로 Arena의 장점이 감소.
그럼에도 불구하고 약간의 개선(약 **8~10%**)이 유지된다.  
이는 Arena 내부 포인터가 **더 밀집되어 TLB 캐시 효율이 높기 때문이다.**

---

### 메모리 사용량 (RSS)
두 방식 모두 **비슷한 RSS(프로세스 메모리 사용량)** 을 보인다.
OS의 페이지 단위 관리 특성상 **유의미한 차이는 없다.**
