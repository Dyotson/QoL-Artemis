#include "ADCReader.h"
#include <algorithm>

ADCReader::ADCReader(ADC& adc, adc_channel_t chan, adc_cali_handle_t cali, float offset, float factor, float emaA, float min, float max)
		: adc(adc), chan(chan), cali(cali), offset(offset), factor(factor), emaA(emaA), min(min), max(max){

}

float ADCReader::sample(){
	int raw = 0;
	if(adc.read(chan, raw, cali) != ESP_OK){
		return getValue();
	}

	if(value == -1 || emaA == 1){
		value = raw;
	}else{
		value = value * (1.0f - emaA) + emaA * (float) raw;
	}

	return getValue();
}

float ADCReader::getValue() const{
	const float adjusted = value * factor + offset + moreOffset;

	if(max == 0 && min == 0){
		return adjusted;
	}

	float minimum = min;
	float maximum = max;

	if(min > max){
		std::swap(minimum, maximum);
	}

	float val = std::clamp(adjusted, minimum, maximum);
	val = (val - minimum) / (maximum - minimum);
	val = std::clamp(val * 100.0f, 0.0f, 100.0f);

	if(min > max){
		val = 100.0f - val;
	}

	return val;
}

void ADCReader::resetEma(){
	value = -1;

	// A single conversion right after an ADC re-init (e.g. waking from light sleep) is
	// occasionally way off. Seed the filter with the median of a few conversions instead,
	// so one bad read can't be mistaken for a critically low battery.
	constexpr int N = 7;
	int reads[N];
	int got = 0;
	for(int i = 0; i < N; i++){
		int raw = 0;
		if(adc.read(chan, raw, cali) == ESP_OK){
			reads[got++] = raw;
		}
	}

	if(got == 0) return;

	std::nth_element(reads, reads + got / 2, reads + got);
	value = (float) reads[got / 2];
}

void ADCReader::setMoreOffset(float offset){
	moreOffset = offset;
}

void ADCReader::setEMAFactor(float factor){
	emaA = factor;
	resetEma();
}
