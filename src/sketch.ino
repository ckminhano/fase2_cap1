/*
 * ============================================================
 *  FarmTech Solutions - Fase 2
 *  Sistema de Irrigação Inteligente + Integração Meteorológica
 * ============================================================
 *
 *  Cultura escolhida: MILHO (Zea mays)
 *
 *  Faixas ideais para a cultura:
 *    - Umidade do solo: 60% a 80%
 *    - pH do solo (simulado via LDR): 5.5 a 6.8 (levemente ácido)
 *    - Nutriente N (Nitrogênio): alto/presente (crítico para milho)
 *    - Nutriente P (Fósforo): presente
 *    - Nutriente K (Potássio): presente
 *
 *  Simulações / substituições didáticas (Wokwi):
 *    - Botão verde N  -> simula sensor de Nitrogênio (true/false)
 *    - Botão verde P  -> simula sensor de Fósforo    (true/false)
 *    - Botão verde K  -> simula sensor de Potássio   (true/false)
 *    - Sensor LDR     -> simula sensor de pH do solo (0 a 14)
 *    - Sensor DHT22   -> simula sensor de umidade do solo (%)
 *    - Relé azul      -> simula bomba d'água da irrigação
 *    - LED vermelho   -> indicador visual "Bomba LIGADA"
 *
 *  Pinagem (ESP32 DevKit v1):
 *    - GPIO 15 -> DHT22 (dados)
 *    - GPIO 34 -> LDR (entrada analógica, somente input)
 *    - GPIO 32 -> Botão N
 *    - GPIO 33 -> Botão P
 *    - GPIO 25 -> Botão K
 *    - GPIO 26 -> Relé (bomba)
 *    - GPIO 27 -> LED indicador
 *
 *  Integração com dados meteorológicos (modo manual):
 *    O script `iralem.py` consulta a API pública Open-Meteo (sem
 *    necessidade de API key) e gera um nível de chuva previsto
 *    (0 a 3) que deve ser colado abaixo na constante
 *    NIVEL_CHUVA_PREVISTO:
 *      0 = Sem chuva           -> comportamento normal
 *      1 = Chuva leve          -> irriga normalmente
 *      2 = Chuva moderada      -> só irriga se umidade < 40%
 *      3 = Chuva forte         -> suspende a irrigação
 *
 *  Lógica de decisão da irrigação:
 *    LIGA a bomba se TODAS as condições forem verdadeiras:
 *      1) Umidade do solo < 60%
 *      2) pH entre 5.5 e 6.8
 *      3) Nitrogênio presente E pelo menos 2 dos 3 nutrientes NPK
 *      4) Previsão de chuva permite irrigação (ver tabela acima)
 *    DESLIGA a bomba se:
 *      - Umidade >= 80%  (solo já bem irrigado), OU
 *      - pH fora da faixa, OU
 *      - Nitrogênio ausente / nutrientes insuficientes, OU
 *      - Previsão de chuva forte (nível 3), OU
 *      - Previsão de chuva moderada (nível 2) E umidade >= 40%
 * ============================================================
 */

#include <DHT.h>

// ---------- Definições de pinos ----------
#define PIN_DHT     15
#define PIN_LDR     34
#define PIN_BTN_N   32
#define PIN_BTN_P   33
#define PIN_BTN_K   25
#define PIN_RELE    26
#define PIN_LED     27

// ---------- Tipo do DHT ----------
#define DHT_TYPE DHT22
DHT dht(PIN_DHT, DHT_TYPE);

// ---------- Parâmetros da cultura (MILHO) ----------
const float UMIDADE_MIN        = 60.0;   // abaixo disso, solo seco
const float UMIDADE_MAX        = 80.0;   // acima disso, solo encharcado
const float UMIDADE_CRITICA    = 40.0;   // abaixo disso, irriga mesmo com chuva moderada
const float PH_MIN             = 5.5;
const float PH_MAX             = 6.8;

// ============================================================
//  Integração Meteorológica (preenchido pelo script iralem.py)
//  0=sem chuva | 1=leve | 2=moderada | 3=forte
// ============================================================
const int NIVEL_CHUVA_PREVISTO = 0;

// ---------- Controle de tempo de leitura ----------
unsigned long ultimaLeitura = 0;
const unsigned long INTERVALO_MS = 2000;  // lê sensores a cada 2s

// ---------- Estado da bomba (para log de transições) ----------
bool bombaLigada = false;

// ============================================================
//  Converte leitura analógica do LDR (0..4095 no ESP32) em pH
//  simulado (0.0 a 14.0). Escala linear para fins didáticos.
// ============================================================
float converterLdrParaPh(int leituraAnalogica) {
  float ph = (leituraAnalogica / 4095.0) * 14.0;
  return ph;
}

// ============================================================
//  Lê um botão (INPUT_PULLUP -> pressionado = LOW).
// ============================================================
bool lerBotao(int pino) {
  return digitalRead(pino) == LOW;
}

// ============================================================
//  Verifica se a previsão de chuva permite irrigar agora.
//  Retorna:
//    true  -> previsão liberada (pode irrigar)
//    false -> previsão bloqueia a irrigação
//  Recebe a umidade para permitir o "resgate" em casos extremos
//  (nível 2 + umidade < 40% ainda irriga).
// ============================================================
bool previsaoPermiteIrrigar(float umidade) {
  switch (NIVEL_CHUVA_PREVISTO) {
    case 3:
      // Chuva forte prevista -> nunca irriga (economia de água)
      return false;
    case 2:
      // Chuva moderada -> só irriga se solo estiver muito seco
      return (umidade < UMIDADE_CRITICA);
    case 1:
    case 0:
    default:
      return true;
  }
}

// ============================================================
//  Decide se a bomba deve ligar.
// ============================================================
bool decidirIrrigacao(float umidade, float ph, bool n, bool p, bool k) {
  // Trava de segurança: solo encharcado -> nunca liga
  if (umidade >= UMIDADE_MAX) return false;

  // Trava de previsão meteorológica
  if (!previsaoPermiteIrrigar(umidade)) return false;

  // Condições agronômicas
  bool soloSeco = (umidade < UMIDADE_MIN);
  bool phOk     = (ph >= PH_MIN && ph <= PH_MAX);

  int totalNutrientes = (n ? 1 : 0) + (p ? 1 : 0) + (k ? 1 : 0);
  bool nutrientesOk = n && (totalNutrientes >= 2);

  return soloSeco && phOk && nutrientesOk;
}

// ============================================================
//  Retorna uma descrição textual para o nível de chuva.
// ============================================================
const char* descricaoChuva(int nivel) {
  switch (nivel) {
    case 0: return "Sem chuva prevista";
    case 1: return "Chuva leve prevista";
    case 2: return "Chuva moderada prevista";
    case 3: return "Chuva forte prevista";
    default: return "Nivel desconhecido";
  }
}

// ============================================================
//  Imprime um relatório completo no Monitor Serial.
// ============================================================
void imprimirRelatorio(float umidade, float temperatura, int ldrRaw,
                       float ph, bool n, bool p, bool k, bool ligar) {
  Serial.println(F("==================================================="));
  Serial.println(F(" FarmTech Solutions | Monitoramento da Lavoura"));
  Serial.println(F("==================================================="));

  Serial.print(F(" Umidade do solo (DHT22): "));
  if (isnan(umidade)) {
    Serial.println(F("ERRO de leitura"));
  } else {
    Serial.print(umidade, 1);
    Serial.println(F(" %"));
  }

  Serial.print(F(" Temperatura (informativo): "));
  if (isnan(temperatura)) {
    Serial.println(F("ERRO de leitura"));
  } else {
    Serial.print(temperatura, 1);
    Serial.println(F(" C"));
  }

  Serial.print(F(" pH do solo (LDR simulado): "));
  Serial.print(ph, 2);
  Serial.print(F("  [leitura bruta="));
  Serial.print(ldrRaw);
  Serial.println(F("]"));

  Serial.print(F(" Nitrogenio (N): "));
  Serial.println(n ? F("PRESENTE") : F("AUSENTE"));
  Serial.print(F(" Fosforo    (P): "));
  Serial.println(p ? F("PRESENTE") : F("AUSENTE"));
  Serial.print(F(" Potassio   (K): "));
  Serial.println(k ? F("PRESENTE") : F("AUSENTE"));

  Serial.print(F(" Previsao meteorologica: nivel "));
  Serial.print(NIVEL_CHUVA_PREVISTO);
  Serial.print(F(" ("));
  Serial.print(descricaoChuva(NIVEL_CHUVA_PREVISTO));
  Serial.println(F(")"));

  Serial.print(F(" >> Bomba d'agua: "));
  Serial.println(ligar ? F("LIGADA  (irrigando)") : F("DESLIGADA"));
  Serial.println();
}

// ============================================================
//                         SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(F("Inicializando FarmTech Solutions - Fase 2..."));

  pinMode(PIN_BTN_N, INPUT_PULLUP);
  pinMode(PIN_BTN_P, INPUT_PULLUP);
  pinMode(PIN_BTN_K, INPUT_PULLUP);
  pinMode(PIN_LDR,   INPUT);

  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_LED,  OUTPUT);
  digitalWrite(PIN_RELE, LOW);
  digitalWrite(PIN_LED,  LOW);

  dht.begin();

  Serial.println(F("Cultura configurada: MILHO"));
  Serial.print(F("Umidade ideal: "));
  Serial.print(UMIDADE_MIN); Serial.print(F("% a "));
  Serial.print(UMIDADE_MAX); Serial.println(F("%"));
  Serial.print(F("pH ideal: "));
  Serial.print(PH_MIN); Serial.print(F(" a "));
  Serial.println(PH_MAX);
  Serial.print(F("Previsao meteorologica carregada: nivel "));
  Serial.print(NIVEL_CHUVA_PREVISTO);
  Serial.print(F(" ("));
  Serial.print(descricaoChuva(NIVEL_CHUVA_PREVISTO));
  Serial.println(F(")"));
  Serial.println(F("Sistema pronto.\n"));
}

// ============================================================
//                         LOOP
// ============================================================
void loop() {
  unsigned long agora = millis();
  if (agora - ultimaLeitura < INTERVALO_MS) return;
  ultimaLeitura = agora;

  float umidade     = dht.readHumidity();
  float temperatura = dht.readTemperature();

  if (isnan(umidade)) {
    Serial.println(F("[AVISO] Falha ao ler DHT22. Tentando novamente..."));
    return;
  }

  int   ldrRaw = analogRead(PIN_LDR);
  float ph     = converterLdrParaPh(ldrRaw);

  bool n = lerBotao(PIN_BTN_N);
  bool p = lerBotao(PIN_BTN_P);
  bool k = lerBotao(PIN_BTN_K);

  bool ligar = decidirIrrigacao(umidade, ph, n, p, k);

  digitalWrite(PIN_RELE, ligar ? HIGH : LOW);
  digitalWrite(PIN_LED,  ligar ? HIGH : LOW);

  if (ligar != bombaLigada) {
    Serial.print(F("### Transicao: bomba "));
    Serial.println(ligar ? F("LIGOU") : F("DESLIGOU"));
    bombaLigada = ligar;
  }

  imprimirRelatorio(umidade, temperatura, ldrRaw, ph, n, p, k, ligar);
}
