"""
============================================================
 FarmTech Solutions - Fase 2
 Integração com API pública de clima (Open-Meteo)
============================================================

Este script consulta a API pública Open-Meteo para obter a
previsão meteorológica de uma cidade configurada, analisa as
próximas 24 horas e calcula um "nível de chuva previsto" de
0 a 3 que será usado pelo ESP32 para ajustar a lógica de
irrigação.

Escala de chuva (definida pelo grupo):
    0 = Sem chuva                    -> sem ajuste na irrigação
    1 = Chuva leve prevista          -> irriga normalmente
    2 = Chuva moderada prevista      -> só irriga se solo muito seco (< 40%)
    3 = Chuva forte prevista         -> suspende a irrigação

Modo de uso:
    1) (Opcional) Ajuste as coordenadas em CIDADE_LAT / CIDADE_LON.
    2) Execute:  python iralem.py
    3) Copie o bloco de código C++ impresso no final e cole no
       sketch.ino, substituindo a constante NIVEL_CHUVA_PREVISTO.

Requisitos:
    pip install requests
============================================================
"""

import sys
from datetime import datetime, timedelta

try:
    import requests
except ImportError:
    print("ERRO: a biblioteca 'requests' não está instalada.")
    print("Instale com:  pip install requests")
    sys.exit(1)


# ============================================================
# Configuração
# ============================================================

# Coordenadas padrão: Santo André, SP, Brasil
# (ajuste livremente para a cidade da sua lavoura)
CIDADE_NOME = "Santo Andre, SP, Brasil"
CIDADE_LAT = -23.6533
CIDADE_LON = -46.5322

HORAS_PREVISAO = 24  # janela de análise em horas

URL_BASE = "https://api.open-meteo.com/v1/forecast"


# ============================================================
# Consulta à API
# ============================================================

def consultar_previsao(lat: float, lon: float) -> dict:
    """
    Consulta a Open-Meteo e retorna o JSON de previsão horária.
    Pede precipitação em mm e probabilidade de precipitação em %.
    """
    parametros = {
        "latitude":   lat,
        "longitude":  lon,
        "hourly":     "precipitation,precipitation_probability",
        "forecast_days": 2,   # hoje + amanhã cobre a janela de 24h
        "timezone":   "auto",
    }
    resposta = requests.get(URL_BASE, params=parametros, timeout=10)
    resposta.raise_for_status()
    return resposta.json()


# ============================================================
# Análise da previsão
# ============================================================

def analisar_previsao(dados: dict, horas: int = HORAS_PREVISAO) -> dict:
    """
    Analisa as próximas `horas` horas da previsão horária e
    retorna um resumo: volume total de chuva (mm), maior volume
    em uma única hora e maior probabilidade de precipitação.
    """
    hourly = dados.get("hourly", {})
    tempos = hourly.get("time", [])
    chuvas = hourly.get("precipitation", [])
    probs = hourly.get("precipitation_probability", [])

    # A Open-Meteo devolve a série a partir das 00:00 do dia atual
    # no fuso local. Precisamos filtrar a partir de "agora".
    agora = datetime.now().replace(minute=0, second=0, microsecond=0)
    limite = agora + timedelta(hours=horas)

    volume_total = 0.0
    volume_pico = 0.0
    prob_maxima = 0.0
    blocos_analisados = []

    for i, t_str in enumerate(tempos):
        try:
            t = datetime.fromisoformat(t_str)
        except ValueError:
            continue

        # Considera apenas o intervalo [agora, agora + HORAS_PREVISAO)
        if t < agora or t >= limite:
            continue

        chuva = chuvas[i] if i < len(chuvas) and chuvas[i] is not None else 0.0
        prob = probs[i] if i < len(probs) and probs[i] is not None else 0.0
        prob_frac = prob / 100.0  # converte para 0.0 - 1.0

        volume_total += chuva
        volume_pico = max(volume_pico, chuva)
        prob_maxima = max(prob_maxima, prob_frac)

        blocos_analisados.append({
            "data": t.strftime("%d/%m %H:%M"),
            "chuva_mm": chuva,
            "prob": prob_frac,
        })

    return {
        "volume_total": volume_total,
        "volume_pico":  volume_pico,
        "prob_maxima":  prob_maxima,
        "blocos":       blocos_analisados,
    }


# ============================================================
# Classificação em nível 0-3
# ============================================================

def classificar_nivel_chuva(resumo: dict) -> int:
    """
    Converte o resumo meteorológico em um nível de 0 a 3.

    Critérios (cumulativos nas próximas ~24h):
      - Nível 3 (forte):    volume_total >= 10mm  OU volume_pico >= 5mm
      - Nível 2 (moderada): volume_total >= 3mm   OU volume_pico >= 2mm
      - Nível 1 (leve):     volume_total > 0mm    OU prob_maxima >= 0.5
      - Nível 0:            caso contrário
    """
    total = resumo["volume_total"]
    pico = resumo["volume_pico"]
    prob = resumo["prob_maxima"]

    if total >= 10.0 or pico >= 5.0:
        return 3
    if total >= 3.0 or pico >= 2.0:
        return 2
    if total > 0.0 or prob >= 0.5:
        return 1
    return 0


def descricao_nivel(nivel: int) -> str:
    return {
        0: "Sem chuva prevista",
        1: "Chuva leve prevista",
        2: "Chuva moderada prevista",
        3: "Chuva forte prevista",
    }.get(nivel, "Desconhecido")


# ============================================================
# Saída formatada
# ============================================================

def imprimir_relatorio(cidade: str, resumo: dict, nivel: int) -> None:
    print()
    print("=" * 60)
    print(" FarmTech Solutions | Relatório Meteorológico")
    print(" Fonte: Open-Meteo (https://open-meteo.com)")
    print("=" * 60)
    print(f" Localidade analisada: {cidade}")
    print(f" Coordenadas: lat={CIDADE_LAT}, lon={CIDADE_LON}")
    print(f" Janela de análise: próximas {HORAS_PREVISAO} horas")
    print("-" * 60)
    print(f" Volume total de chuva previsto: {resumo['volume_total']:.2f} mm")
    print(f" Pico em uma hora:               {resumo['volume_pico']:.2f} mm")
    print(f" Probabilidade máxima de chuva:  {resumo['prob_maxima']*100:.0f}%")
    print(f" Horas analisadas:               {len(resumo['blocos'])}")
    print("-" * 60)
    print(" Detalhamento por hora (apenas horas com chuva > 0 ou prob >= 30%):")
    destaque = [b for b in resumo["blocos"]
                if b["chuva_mm"] > 0 or b["prob"] >= 0.3]
    if not destaque:
        print("   (nenhuma hora com chuva significativa na janela)")
    else:
        for b in destaque:
            print(f"   {b['data']}  |  {b['chuva_mm']:5.2f} mm  |  "
                  f"pop={b['prob']*100:3.0f}%")
    print("=" * 60)
    print(f" >> NIVEL DE CHUVA CALCULADO: {nivel}  ({descricao_nivel(nivel)})")
    print("=" * 60)


def imprimir_bloco_cpp(nivel: int) -> None:
    """Imprime o trecho de código pronto para colar no sketch.ino."""
    print()
    print("-" * 60)
    print(" Copie o bloco abaixo e cole no sketch.ino")
    print(" (substituindo a definição atual de NIVEL_CHUVA_PREVISTO):")
    print("-" * 60)
    print()
    print("// Atualizado automaticamente via iralem.py (Open-Meteo)")
    print(f"// Gerado em: {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}")
    print(f"// Local: {CIDADE_NOME}")
    print(f"// 0=sem chuva | 1=leve | 2=moderada | 3=forte")
    print(f"const int NIVEL_CHUVA_PREVISTO = {nivel};")
    print()
    print("-" * 60)


# ============================================================
# Função principal
# ============================================================

def main() -> int:
    print(f"Consultando Open-Meteo para {CIDADE_NOME}...")
    try:
        dados = consultar_previsao(CIDADE_LAT, CIDADE_LON)
    except requests.HTTPError as e:
        print(f"ERRO HTTP: {e}")
        return 1
    except requests.RequestException as e:
        print(f"ERRO de conexão: {e}")
        return 1

    resumo = analisar_previsao(dados)

    if not resumo["blocos"]:
        print("AVISO: nenhum dado horário retornado para a janela analisada.")
        print("Verifique as coordenadas informadas.")
        return 1

    nivel = classificar_nivel_chuva(resumo)

    imprimir_relatorio(CIDADE_NOME, resumo, nivel)
    imprimir_bloco_cpp(nivel)

    return 0


if __name__ == "__main__":
    sys.exit(main())
