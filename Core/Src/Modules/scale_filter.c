#include "scale_filter.h"
#include <math.h>
#include "ads1232_driver.h"  // para usar ADS1232_ConvertToGrams()

// ganho local (gramas por count) estimado por diferença finita
static float local_g_per_count(int32_t counts_centro)
{
    // passo pequeno em counts; aproveita sua curva para estimar o ganho local
    float g1 = ADS1232_ConvertToGrams(counts_centro);
    float g2 = ADS1232_ConvertToGrams(counts_centro + 64);
    float dg = g2 - g1;
    if (dg <= 0.0f) {
        // fallback conservador se algo estranho acontecer
        return 1.0f / 6200.0f;
    }
    return dg / 64.0f;
}

void ScaleFilter_Init(ScaleFilter* sf, int32_t initial_counts)
{
    sf->idx       = 0;
    sf->filled    = 1;
    for (int i = 0; i < SCALE_FILTER_WIN_SIZE; i++) {
        sf->buffer[i] = initial_counts;
    }
    // valores default razoáveis (você pode sobrescrever com SetThresholds)
    sf->stability_sigma_g = 0.020f;
    sf->stability_slope_g = 0.003f;
    sf->step_threshold_g  = 0.30f;
    sf->first_avg = 1;
    sf->prev_avg_g = 0.0f;
}

void ScaleFilter_ResetWithOffset(ScaleFilter* sf, int32_t new_offset_counts)
{
    ScaleFilter_Init(sf, new_offset_counts);
}

void ScaleFilter_SetThresholds(ScaleFilter* sf,
                               float sigma_g_thr,
                               float slope_g_thr,
                               float step_thr_g)
{
    sf->stability_sigma_g = sigma_g_thr;
    sf->stability_slope_g = slope_g_thr;
    sf->step_threshold_g  = step_thr_g;
}

void ScaleFilter_Push(ScaleFilter* sf, int32_t new_counts, ScaleFilterOut* out)
{
    // insere no buffer circular
    sf->buffer[sf->idx] = new_counts;
    sf->idx = (sf->idx + 1) % SCALE_FILTER_WIN_SIZE;
    // como o Init já preencheu tudo, "filled" fica 1 desde o começo

    // cálculo de média (counts)
    long long sum = 0;
    for (int i = 0; i < SCALE_FILTER_WIN_SIZE; i++) {
        sum += sf->buffer[i];
    }
    float avg_counts = (float)((double)sum / (double)SCALE_FILTER_WIN_SIZE);

    // desvio padrão (counts)
    long long sum_sq_diff = 0;
    for (int i = 0; i < SCALE_FILTER_WIN_SIZE; i++) {
        long long diff = sf->buffer[i] - (long long)avg_counts;
        sum_sq_diff += diff * diff;
    }
    float sigma_counts = sqrtf((float)((double)sum_sq_diff / (double)SCALE_FILTER_WIN_SIZE));

    // regressão linear simples (slope em counts por amostra)
    long long sum_xy = 0, sum_x = 0, sum_y = 0, sum_x2 = 0;
    for (int i = 0; i < SCALE_FILTER_WIN_SIZE; i++) {
        sum_xy += (long long)i * sf->buffer[i];
        sum_x  += i;
        sum_y  += sf->buffer[i];
        sum_x2 += (long long)i * i;
    }
    long long N = SCALE_FILTER_WIN_SIZE;
    long long num = (N * sum_xy - sum_x * sum_y);
    long long den = (N * sum_x2 - sum_x * sum_x);
    float slope_counts = 0.0f;
    if (den != 0) {
        slope_counts = (float)((double)num / (double)den);
    }

    // conversões para gramas
    float gpc = local_g_per_count((int32_t)avg_counts);  // g/count
    float avg_grams   = ADS1232_ConvertToGrams((int32_t)avg_counts);
    float sigma_grams = sigma_counts * gpc;
    float slope_grams = slope_counts * gpc;

    // estabilidade e detecção de degrau
    uint8_t is_stable = (sigma_grams < sf->stability_sigma_g) &&
                        (fabsf(slope_grams) < sf->stability_slope_g);

    uint8_t step = 0;
    if (sf->first_avg) {
        sf->first_avg = 0;
        sf->prev_avg_g = avg_grams;
    } else {
        if (fabsf(avg_grams - sf->prev_avg_g) > sf->step_threshold_g) {
            step = 1;
        }
        sf->prev_avg_g = avg_grams;
    }

    // saída
    out->avg_counts   = avg_counts;
    out->avg_grams    = avg_grams;
    out->sigma_counts = sigma_counts;
    out->sigma_grams  = sigma_grams;
    out->slope_counts = slope_counts;
    out->slope_grams  = slope_grams;
    out->is_stable    = is_stable;
    out->step_detected= step;
}
