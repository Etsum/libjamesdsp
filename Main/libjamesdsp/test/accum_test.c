// Host test for accumulate mode. Run with run.sh in this folder.
// Write mode must be the same as the base build. Accumulate mode must equal prior + write output.
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <dlfcn.h>
#include "essential.h"

typedef int32_t (*create_t)(const effect_uuid_t *, int32_t, int32_t, effect_handle_t *);

static effect_handle_t mk(const char *so, int accessMode, uint32_t fs) {
	void *h = dlopen(so, RTLD_NOW | RTLD_LOCAL);
	if (!h) { fprintf(stderr, "%s\n", dlerror()); exit(1); }
	create_t create = (create_t)dlsym(h, "EffectCreate");
	effect_handle_t e; create(NULL, 81, 1, &e);
	effect_config_t c; memset(&c, 0, sizeof c);
	c.inputCfg.samplingRate = c.outputCfg.samplingRate = fs;
	c.inputCfg.channels = c.outputCfg.channels = AUDIO_CHANNEL_OUT_STEREO;
	c.inputCfg.format = c.outputCfg.format = AUDIO_FORMAT_PCM_FLOAT;
	c.inputCfg.accessMode = EFFECT_BUFFER_ACCESS_READ;
	c.outputCfg.accessMode = accessMode;
	c.inputCfg.mask = c.outputCfg.mask = EFFECT_CONFIG_ALL;
	int32_t r = 0; uint32_t rs = 4;
	(*e)->command(e, EFFECT_CMD_SET_CONFIG, sizeof c, &c, &rs, &r);
	if (r) { fprintf(stderr, "SET_CONFIG %d\n", r); exit(1); }
	(*e)->command(e, EFFECT_CMD_ENABLE, 0, NULL, &rs, &r);
	int32_t p[5] = {0, 4, 2, 0, 0};             // effect_param_t: status, psize, vsize, cmd, int16 value
	p[3] = 112; ((int16_t *)p)[8] = 12;  (*e)->command(e, EFFECT_CMD_SET_PARAM, sizeof p, p, &rs, &r); // bass max gain
	p[3] = 1201; ((int16_t *)p)[8] = 1;  (*e)->command(e, EFFECT_CMD_SET_PARAM, sizeof p, p, &rs, &r); // bass on
	return e;
}

int main(int argc, char **argv) {
	const char *orig = argv[1], *pat = argv[2];
	uint32_t rates[] = {48000, 192000};
	size_t blocks[] = {3840, 7680, 192, 7680};      // includes growth after a smaller block
	int fails = 0;
	for (int ri = 0; ri < 2; ri++) {
		uint32_t fs = rates[ri];
		effect_handle_t eo = mk(orig, EFFECT_BUFFER_ACCESS_WRITE, fs);
		effect_handle_t ew = mk(pat, EFFECT_BUFFER_ACCESS_WRITE, fs);
		effect_handle_t ea = mk(pat, EFFECT_BUFFER_ACCESS_ACCUMULATE, fs);
		double t = 0, maxW = 0, maxA = 0, energy = 0;
		for (int it = 0; it < 40; it++) {
			size_t n = blocks[it % 4];
			float *in = malloc(n * 8), *in2 = malloc(n * 8), *in3 = malloc(n * 8);
			float *oo = malloc(n * 8), *ow = malloc(n * 8), *oa = malloc(n * 8), *prior = malloc(n * 8);
			for (size_t i = 0; i < n; i++, t += 1.0 / fs) {
				in[2*i] = 0.3f * sinf(2 * M_PI * 60 * t); in[2*i+1] = 0.3f * sinf(2 * M_PI * 1000 * t);
			}
			for (size_t i = 0; i < 2 * n; i++) { prior[i] = 0.1f * sinf(i * 0.37f); oa[i] = prior[i]; oo[i] = ow[i] = 7.0f; }
			memcpy(in2, in, n * 8); memcpy(in3, in, n * 8);
			audio_buffer_t bi = {n, {in}}, bi2 = {n, {in2}}, bi3 = {n, {in3}};
			audio_buffer_t bo = {n, {oo}}, bw = {n, {ow}}, ba = {n, {oa}};
			(*eo)->process(eo, &bi, &bo);
			(*ew)->process(ew, &bi2, &bw);
			(*ea)->process(ea, &bi3, &ba);
			for (size_t i = 0; i < 2 * n; i++) {
				double dW = fabs(oo[i] - ow[i]), dA = fabs(oa[i] - (prior[i] + oo[i]));
				if (dW > maxW) maxW = dW;
				if (dA > maxA) maxA = dA;
				energy += oo[i] * oo[i];
			}
			free(in); free(in2); free(in3); free(oo); free(ow); free(oa); free(prior);
		}
		printf("fs=%u  WRITE orig-vs-patched max diff %.3g  ACCUMULATE max err %.3g  output energy %.3g\n", fs, maxW, maxA, energy);
		if (maxW != 0 || maxA > 1e-6 || energy == 0) fails++;
	}
	printf(fails ? "FAIL\n" : "PASS\n");
	return fails;
}
