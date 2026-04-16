#include <stdint.h>
#include <math.h>
#include <x86intrin.h>   // __rdtsc, SSE intrinsics

typedef struct { float w,x,y,z; } quatf;

static inline uint64_t rdtsc64(void){ return __rdtsc(); }

/* Fast normalize */
static inline quatf normalize_quat(quatf q){
    __m128 v = _mm_set_ps(q.z,q.y,q.x,q.w);
    __m128 sq = _mm_mul_ps(v,v);
    __m128 t1 = _mm_hadd_ps(sq,sq);
    __m128 t2 = _mm_hadd_ps(t1,t1);
    float n2 = _mm_cvtss_f32(t2);
    __m128 rs = _mm_rsqrt_ss(_mm_set_ss(n2));
    float invn = _mm_cvtss_f32(rs);
    q.w*=invn; q.x*=invn; q.y*=invn; q.z*=invn;
    return q;
}

static inline quatf accel_tilt_quat(const float a[3]){
    float invn = 1.0f / sqrtf(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
    float gx=-a[0]*invn, gy=-a[1]*invn, gz=-a[2]*invn;
    float pitch = atan2f(gx, sqrtf(gy*gy+gz*gz));
    float roll  = atan2f(gy, gz);
    float cp=cosf(pitch*0.5f), sp=sinf(pitch*0.5f);
    float cr=cosf(roll*0.5f),  sr=sinf(roll*0.5f);
    return (quatf){
        cr*cp,               // w (yaw=0)
        sr*cp,
        cr*sp,
        0.0f
    };
}

void imu_update(float gyro[3], float accel[3],
                uint64_t *ts_prev, double tsc_freq, quatf *q_state)
{
    _mm_setcsr(0x1F80);

    uint64_t tnow = rdtsc64();
    double dt = (double)(tnow - *ts_prev) / tsc_freq;
    *ts_prev = tnow;

    quatf q = *q_state;
    float wx=gyro[0], wy=gyro[1], wz=gyro[2];

    quatf qdot = {
        -0.5f*(q.x*wx + q.y*wy + q.z*wz),
         0.5f*(q.w*wx + q.y*wz - q.z*wy),
         0.5f*(q.w*wy + q.z*wx - q.x*wz),
         0.5f*(q.w*wz + q.x*wy - q.y*wx)
    };

    q.w += (float)(dt*qdot.w);
    q.x += (float)(dt*qdot.x);
    q.y += (float)(dt*qdot.y);
    q.z += (float)(dt*qdot.z);

    q = normalize_quat(q);
    quatf qa = accel_tilt_quat(accel);

    const float alpha = 0.02f;
    q.w = (1-alpha)*q.w + alpha*qa.w;
    q.x = (1-alpha)*q.x + alpha*qa.x;
    q.y = (1-alpha)*q.y + alpha*qa.y;
    q.z = (1-alpha)*q.z + alpha*qa.z;

    *q_state = normalize_quat(q);
}