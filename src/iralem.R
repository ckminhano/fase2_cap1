# ============================================================
#  FarmTech Solutions - Fase 2
#  Analise Estatistica Basica em R - Apoio a Decisao de Irrigacao
# ============================================================
#
#  Este script aplica conceitos BASICOS de estatistica descritiva
#  para apoiar a decisao de irrigar a lavoura de milho. Usamos
#  apenas ferramentas simples e acessiveis:
#
#    - Medidas de tendencia central: media, mediana
#    - Medidas de dispersao: desvio padrao, quartis
#    - Contagem e frequencias
#    - Score ponderado simples para decisao final
#
#  Fluxo:
#    1) Simula/carrega um historico de leituras dos sensores.
#    2) Calcula estatisticas descritivas do historico.
#    3) Compara as leituras ATUAIS com o historico.
#    4) Calcula um SCORE de irrigacao (soma ponderada).
#    6) Recomenda ligar ou desligar a bomba.
#
# ============================================================

set.seed(42)  # reprodutibilidade

# ============================================================
#  1) HISTORICO DE LEITURAS DOS SENSORES
# ============================================================
#  Simulamos 30 dias de leituras de uma fazenda de milho.
#  Em um cenario real, esses dados viriam de um banco de dados
#  com leituras do ESP32 salvas ao longo do tempo.
# ============================================================

cat("=============================================================\n")
cat(" FarmTech Solutions - Analise Estatistica da Lavoura\n")
cat("=============================================================\n\n")

n_dias <- 30

historico <- data.frame(
  dia         = 1:n_dias,
  umidade     = round(rnorm(n_dias, mean = 55, sd = 12), 1),  # media 55%
  ph          = round(rnorm(n_dias, mean = 6.2, sd = 0.5), 2),
  temperatura = round(rnorm(n_dias, mean = 24, sd = 3), 1),
  N_presente  = rbinom(n_dias, 1, 0.75),
  P_presente  = rbinom(n_dias, 1, 0.70),
  K_presente  = rbinom(n_dias, 1, 0.70)
)

# Garante valores realistas (umidade nao pode passar de 100% nem ser negativa)
historico$umidade <- pmin(pmax(historico$umidade, 10), 95)
historico$ph      <- pmin(pmax(historico$ph, 3), 10)

cat(">> Historico de leituras (ultimos 30 dias) - 6 primeiros registros:\n")
print(head(historico))
cat("\n")

# ============================================================
#  2) ESTATISTICAS DESCRITIVAS DO HISTORICO
# ============================================================
#  Aqui aplicamos conceitos basicos de estatistica: medidas
#  de tendencia central (media, mediana) e de dispersao
#  (desvio padrao, minimo, maximo, quartis).
# ============================================================

cat("=============================================================\n")
cat(" ESTATISTICAS DESCRITIVAS DO HISTORICO\n")
cat("=============================================================\n")

cat("\n--- UMIDADE do solo (%) ---\n")
cat(sprintf("  Media:         %.2f\n", mean(historico$umidade)))
cat(sprintf("  Mediana:       %.2f\n", median(historico$umidade)))
cat(sprintf("  Desvio padrao: %.2f\n", sd(historico$umidade)))
cat(sprintf("  Minimo:        %.2f\n", min(historico$umidade)))
cat(sprintf("  Maximo:        %.2f\n", max(historico$umidade)))
cat(sprintf("  1o quartil:    %.2f\n", quantile(historico$umidade, 0.25)))
cat(sprintf("  3o quartil:    %.2f\n", quantile(historico$umidade, 0.75)))

cat("\n--- pH do solo ---\n")
cat(sprintf("  Media:         %.2f\n", mean(historico$ph)))
cat(sprintf("  Mediana:       %.2f\n", median(historico$ph)))
cat(sprintf("  Desvio padrao: %.2f\n", sd(historico$ph)))
cat(sprintf("  Minimo:        %.2f\n", min(historico$ph)))
cat(sprintf("  Maximo:        %.2f\n", max(historico$ph)))

cat("\n--- Presenca de nutrientes (%) ---\n")
cat(sprintf("  Nitrogenio: %.1f%%\n", 100 * mean(historico$N_presente)))
cat(sprintf("  Fosforo:    %.1f%%\n", 100 * mean(historico$P_presente)))
cat(sprintf("  Potassio:   %.1f%%\n", 100 * mean(historico$K_presente)))

# Contagem de dias fora da faixa ideal (para milho: umidade 60-80%, pH 5.5-6.8)
dias_secos     <- sum(historico$umidade < 60)
dias_ideais    <- sum(historico$umidade >= 60 & historico$umidade <= 80)
dias_saturados <- sum(historico$umidade > 80)

cat(sprintf("\n--- Distribuicao de dias por faixa de umidade ---\n"))
cat(sprintf("  Solo seco    (< 60%%):  %d dias (%.1f%%)\n",
            dias_secos, 100 * dias_secos / n_dias))
cat(sprintf("  Faixa ideal  (60-80%%): %d dias (%.1f%%)\n",
            dias_ideais, 100 * dias_ideais / n_dias))
cat(sprintf("  Solo saturado(> 80%%): %d dias (%.1f%%)\n",
            dias_saturados, 100 * dias_saturados / n_dias))

# ============================================================
#  3) LEITURAS ATUAIS DOS SENSORES
# ============================================================
#  Altere estes valores para refletir a leitura atual do Wokwi.
# ============================================================

leitura_atual <- list(
  umidade     = 45.0,   # <- leitura do DHT22 (%)
  ph          = 6.2,    # <- leitura do LDR convertida (0-14)
  N_presente  = 1,      # <- botao N  (1 = pressionado)
  P_presente  = 1,      # <- botao P
  K_presente  = 0,      # <- botao K
  nivel_chuva = 0       # <- nivel de chuva do iralem.py (0-3)
)

cat("\n=============================================================\n")
cat(" LEITURAS ATUAIS vs HISTORICO\n")
cat("=============================================================\n")

# Comparacao da umidade atual com o historico usando Z-score simples
# Z = (valor - media) / desvio_padrao
# Mostra quantos desvios padrao a leitura atual esta da media historica
z_umidade <- (leitura_atual$umidade - mean(historico$umidade)) /
              sd(historico$umidade)

cat(sprintf("\nUmidade atual: %.1f%% | media historica: %.1f%%\n",
            leitura_atual$umidade, mean(historico$umidade)))
cat(sprintf("  -> Z-score: %.2f (", z_umidade))
if (abs(z_umidade) < 1) {
  cat("proximo da media)\n")
} else if (z_umidade < 0) {
  cat("abaixo da media - solo mais seco que o normal)\n")
} else {
  cat("acima da media - solo mais umido que o normal)\n")
}

cat(sprintf("\npH atual: %.2f | media historica: %.2f\n",
            leitura_atual$ph, mean(historico$ph)))

# Percentil da umidade atual no historico
percentil <- mean(historico$umidade <= leitura_atual$umidade) * 100
cat(sprintf("\nA umidade atual esta no percentil %.0f%% do historico.\n",
            percentil))
cat("(Se for um percentil baixo, significa que hoje esta mais seco que\n")
cat(" na maioria dos dias observados - indicacao para irrigar.)\n")

# ============================================================
#  4) SCORE DE IRRIGACAO (soma ponderada simples)
# ============================================================
#  Calculamos um score de 0 a 100 indicando a "necessidade" de
#  irrigar. Quanto maior o score, mais a irrigacao e recomendada.
#  Cada fator contribui com pontos positivos (pede irrigacao) ou
#  negativos (contra-indica irrigacao). E aritmetica simples.
# ============================================================

cat("\n=============================================================\n")
cat(" CALCULO DO SCORE DE IRRIGACAO\n")
cat("=============================================================\n")

score <- 0

# (a) Contribuicao da umidade (0 a 40 pontos)
# Quanto mais seco o solo, mais pontos
if (leitura_atual$umidade < 40) {
  pontos_umidade <- 40
  desc_umidade   <- "solo muito seco (+40)"
} else if (leitura_atual$umidade < 60) {
  pontos_umidade <- 25
  desc_umidade   <- "solo seco (+25)"
} else if (leitura_atual$umidade <= 80) {
  pontos_umidade <- 0
  desc_umidade   <- "solo na faixa ideal (0)"
} else {
  pontos_umidade <- -30
  desc_umidade   <- "solo saturado (-30)"
}
score <- score + pontos_umidade
cat(sprintf("  Umidade (%.1f%%): %s\n", leitura_atual$umidade, desc_umidade))

# (b) Contribuicao do pH (-20 a +10 pontos)
if (leitura_atual$ph >= 5.5 & leitura_atual$ph <= 6.8) {
  pontos_ph <- 10
  desc_ph   <- "pH na faixa ideal para milho (+10)"
} else {
  pontos_ph <- -20
  desc_ph   <- "pH fora da faixa ideal (-20)"
}
score <- score + pontos_ph
cat(sprintf("  pH (%.2f): %s\n", leitura_atual$ph, desc_ph))

# (c) Contribuicao dos nutrientes (0 a 30 pontos)
total_nut <- leitura_atual$N_presente + leitura_atual$P_presente +
             leitura_atual$K_presente
if (leitura_atual$N_presente == 1 & total_nut >= 2) {
  pontos_nut <- 30
  desc_nut   <- sprintf("N presente + %d de 3 nutrientes (+30)", total_nut)
} else if (leitura_atual$N_presente == 1) {
  pontos_nut <- 10
  desc_nut   <- "apenas N presente (+10)"
} else {
  pontos_nut <- -20
  desc_nut   <- "N ausente (-20)"
}
score <- score + pontos_nut
cat(sprintf("  Nutrientes NPK: %s\n", desc_nut))

# (d) Contribuicao da previsao de chuva (-40 a 0 pontos)
if (leitura_atual$nivel_chuva == 3) {
  pontos_chuva <- -40
  desc_chuva   <- "chuva forte prevista (-40)"
} else if (leitura_atual$nivel_chuva == 2) {
  pontos_chuva <- -20
  desc_chuva   <- "chuva moderada prevista (-20)"
} else if (leitura_atual$nivel_chuva == 1) {
  pontos_chuva <- -5
  desc_chuva   <- "chuva leve prevista (-5)"
} else {
  pontos_chuva <- 0
  desc_chuva   <- "sem chuva prevista (0)"
}
score <- score + pontos_chuva
cat(sprintf("  Chuva (nivel %d): %s\n",
            leitura_atual$nivel_chuva, desc_chuva))

cat(sprintf("\n  >> SCORE FINAL: %d pontos\n", score))

# ============================================================
#  5) RECOMENDACAO FINAL
# ============================================================

cat("\n=============================================================\n")
cat(" RECOMENDACAO\n")
cat("=============================================================\n")

if (score >= 40) {
  recomendacao <- "LIGAR A BOMBA"
  motivo <- "o conjunto de fatores indica forte necessidade de irrigacao."
} else if (score >= 20) {
  recomendacao <- "LIGAR A BOMBA (atencao)"
  motivo <- "ha necessidade de irrigacao, mas com margem estreita - monitore."
} else {
  recomendacao <- "MANTER DESLIGADA"
  motivo <- "os fatores nao justificam ligar a bomba neste momento."
}

cat(sprintf("  Decisao: %s\n", recomendacao))
cat(sprintf("  Motivo:  %s\n", motivo))
cat(sprintf("  Score calculado: %d (limiar para ligar: 20)\n", score))
