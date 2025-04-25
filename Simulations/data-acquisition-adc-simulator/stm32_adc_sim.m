% STM32H723ZG ADC Simulation for HP141T Spectrum Analyzer
% Simulates 2x16-bit ADCs in double-interleaved mode (7.2 MSPS) sampling
% the vertical output signal (10 kHz, -0.8V to 0V, level-shifted to 0.8V to 1.6V).

% Step 1: Define the vertical output analog signal
f = 10000;              % Signal frequency (Hz)
A = 0.4;                % Amplitude (V, for 0.8V peak-to-peak)
B = 1.2;                % DC offset (V, level-shifted to 0.8V to 1.6V)
Fs = 14400000;          % Signal generation sampling frequency (Hz, oversampled)
T = 1/Fs;               % Signal sampling period (s)
L = 10000;              % Number of samples (for ~1ms duration)
t = (0:L-1)*T;          % Time vector (s)
Vy = A*sin(2*pi*f*t) + B; % Vertical output sine wave (V)
Vy = Vy + 0.001*randn(size(Vy)); % Add Gaussian noise (1mV RMS)

% Step 2: Characterize the STM32H723ZG ADC
res = 16;               % ADC resolution (16-bit)
Vref = 3.3;             % ADC reference voltage (V, unipolar: 0 to Vref)
Fs_adc = 7200000;       % ADC sampling frequency (Hz, double-interleaved mode)
T_adc = 1/Fs_adc;       % ADC sampling period (s)
qLevels = 2^res;        % Number of quantization levels
qStep = Vref/(qLevels-1); % Quantization step size (V)

% Step 3: Sample the vertical output signal
numSamples = 0:T_adc:max(t); % ADC sampling time points
sampledSig = A*sin(2*pi*f*numSamples) + B; % Sampled signal
sampledSig = sampledSig + 0.001*randn(size(sampledSig)); % Add noise

% Step 4: Quantize the sampled signal
% Clip signal to ADC input range [0, Vref]
sampledSig = max(min(sampledSig, Vref), 0);
qSig = round(sampledSig / qStep) * qStep; % Quantized signal

% Step 5: Simulate double-interleaved mode
% Split samples between ADC1 and ADC2 (alternating)
adc1_samples = qSig(1:2:end); % ADC1 samples (odd indices)
adc2_samples = qSig(2:2:end); % ADC2 samples (even indices)
adc1_times = numSamples(1:2:end);
adc2_times = numSamples(2:2:end);

% Step 6: Encode quantized signal to binary (16-bit unsigned)
digitalSig = zeros(length(qSig), res); % Binary matrix
for i = 1:length(qSig)
    code = round(qSig(i) / qStep);
    digitalSig(i, :) = bitget(code, 1:res);
end

% Step 7: Plot analog, sampled, quantized, and interleaved signals
figure;
% Analog signal
subplot(4,1,1);
plot(t, Vy, 'k', 'LineWidth', 1.5);
title('Analog Vertical Output Signal (Level-Shifted)');
xlabel('Time (s)'); ylabel('Amplitude (V)');
grid on; xlim([0, 2/f]); % Show two periods

% Sampled signal
subplot(4,1,2);
stem(numSamples, sampledSig, 'r', 'Marker', 'o');
title('Sampled Vertical Output Signal (7.2 MSPS)');
xlabel('Time (s)'); ylabel('Amplitude (V)');
grid on; xlim([0, 2/f]);

% Quantized signal
subplot(4,1,3);
stairs(numSamples, qSig, 'b', 'LineWidth', 1.5);
title('Quantized Vertical Output Signal');
xlabel('Time (s)'); ylabel('Amplitude (V)');
grid on; xlim([0, 2/f]);

% Interleaved ADC samples
subplot(4,1,4);
stem(adc1_times, adc1_samples, 'b', 'Marker', 'o', 'DisplayName', 'ADC1');
hold on;
stem(adc2_times, adc2_samples, 'r', 'Marker', '^', 'DisplayName', 'ADC2');
title('Double-Interleaved ADC Samples');
xlabel('Time (s)'); ylabel('Amplitude (V)');
legend('show'); grid on; xlim([0, 2/f]);

% Adjust figure properties
sgtitle('STM32H723ZG ADC Simulation for HP141T Vertical Output');
set(gcf, 'Position', [100, 100, 800, 800]);

% Step 8: Calculate SNR
quantizationError = qSig - sampledSig;
SNR = 10*log10(var(sampledSig)/var(quantizationError));
fprintf('SNR: %.2f dB\n', SNR);