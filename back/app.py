import cv2
import pytesseract
import mysql.connector
import os
import re
import numpy as np
import logging
from flask import Flask, request, jsonify
from dotenv import load_dotenv

load_dotenv()

# Configurar logging
logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s',
                    handlers=[logging.StreamHandler()])
logger = logging.getLogger(__name__)

app = Flask(__name__)

class DetectorPatentes:
    def __init__(self):
        # Patrones de patentes argentinas
        # Formato nuevo: AA 123 AA (dos letras, tres números, dos letras)
        # Formato viejo: AAA 123 (tres letras, tres números)
        self.patron_nuevo = re.compile(r'[A-Z]{2,3}\s*\d{3}\s*[A-Z]{0,3}')
        
        # Patrones más específicos para la limpieza de texto
        self.patron_nuevo_exacto = re.compile(r'([A-Z]{2})\s*(\d{3})\s*([A-Z]{2})')  # AA 123 BB
        self.patron_viejo_exacto = re.compile(r'([A-Z]{3})\s*(\d{3})')  # AAA 123
    
    def limpiar_texto_patente(self, texto):
        if not texto:
            return None
            
        # Eliminar caracteres no alfanuméricos excepto espacios
        texto_limpio = re.sub(r'[^A-Z0-9\s]', '', texto.upper())
        
        # Buscar patrones exactos en el texto
        match_nuevo = self.patron_nuevo_exacto.search(texto_limpio)
        match_viejo = self.patron_viejo_exacto.search(texto_limpio)
        
        if match_nuevo:
            # Formato nuevo: AA 123 BB
            return f"{match_nuevo.group(1)} {match_nuevo.group(2)} {match_nuevo.group(3)}"
        elif match_viejo:
            # Formato viejo: AAA 123
            return f"{match_viejo.group(1)} {match_viejo.group(2)}"
        
        # Si no coincide con los patrones exactos, pero hay algo cercano, intentar extraerlo
        coincidencias = self.patron_nuevo.findall(texto_limpio)
        if coincidencias:
            # Tomar la coincidencia más larga
            mejor_coincidencia = max(coincidencias, key=len)
            
            # Verificar si tiene formato nuevo o viejo y formatear adecuadamente
            partes = re.split(r'\s+', mejor_coincidencia.strip())
            partes = [p for p in partes if p]  # Eliminar elementos vacíos
            
            if len(partes) == 1:
                # Si está todo junto, separarlo
                texto = partes[0]
                if len(texto) >= 7 and texto[:2].isalpha() and texto[2:5].isdigit() and texto[5:].isalpha():
                    # Formato nuevo junto: AB123CD
                    return f"{texto[:2]} {texto[2:5]} {texto[5:7]}"
                elif len(texto) >= 6 and texto[:3].isalpha() and texto[3:].isdigit():
                    # Formato viejo junto: ABC123
                    return f"{texto[:3]} {texto[3:6]}"
            
            # Si ya tiene espacios, pero hay caracteres adicionales
            if len(partes) >= 2:
                # Verificar si el primer elemento tiene 2-3 letras
                if 2 <= len(partes[0]) <= 3 and partes[0].isalpha():
                    letras_inicio = partes[0]
                    
                    # Verificar si el segundo elemento tiene 3 dígitos
                    if len(partes) > 1 and len(partes[1]) >= 3 and partes[1][:3].isdigit():
                        numeros = partes[1][:3]
                        
                        # Verificar si hay un tercer elemento con 2 letras (formato nuevo)
                        if len(partes) > 2 and partes[2].isalpha():
                            letras_fin = partes[2][:2] if len(partes[2]) >= 2 else partes[2]
                            return f"{letras_inicio} {numeros} {letras_fin}"
                        else:
                            # Formato viejo: solo letras y números
                            return f"{letras_inicio} {numeros}"
            
            # Si no se pudo formatear correctamente, devolver la coincidencia original
            return mejor_coincidencia
        
        return None
        
    def detectar_patente(self, imagen_path):
        """Detecta y extrae el texto de la patente en la imagen"""
        # Cargar imagen
        imagen = cv2.imread(imagen_path)
        if imagen is None:
            print(f"No se pudo cargar la imagen en {imagen_path}")
            return None
            
        # Convertir a escala de grises
        gris = cv2.cvtColor(imagen, cv2.COLOR_BGR2GRAY)
        
        # Aplicar desenfoque gaussiano para reducir ruido
        gris = cv2.GaussianBlur(gris, (5, 5), 0)
        
        # Detección de bordes
        bordes = cv2.Canny(gris, 50, 150)
        
        # Dilatación para conectar los bordes
        kernel = np.ones((3, 3), np.uint8)
        bordes = cv2.dilate(bordes, kernel, iterations=1)
        
        # Encontrar contornos
        contornos, _ = cv2.findContours(bordes, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
        
        # Ordenar contornos por área (de mayor a menor)
        contornos = sorted(contornos, key=cv2.contourArea, reverse=True)[:10]
        
        # Priorizar contornos cerca del centro de la imagen
        contornos_filtrados = []
        img_cx, img_cy = imagen.shape[1] // 2, imagen.shape[0] // 2
        for c in contornos:
            x, y, w, h = cv2.boundingRect(c)
            cx, cy = x + w // 2, y + h // 2
            distancia_centro = np.hypot(cx - img_cx, cy - img_cy)
            contornos_filtrados.append((distancia_centro, c))
        contornos = [c for _, c in sorted(contornos_filtrados, key=lambda x: x[0])]
        
        # Lista para almacenar resultados de OCR
        resultados_ocr = []
        
        # Analizar cada contorno como posible patente
        for contorno in contornos:
            # Aproximar el contorno a un polígono
            epsilon = 0.02 * cv2.arcLength(contorno, True)
            approx = cv2.approxPolyDP(contorno, epsilon, True)
            
            # La patente debería ser un rectángulo, así que buscamos polígonos con 4 vértices
            if len(approx) >= 4:
                x, y, w, h = cv2.boundingRect(contorno)
                
                # Verificar proporciones típicas de una patente (ancho/alto)
                proporcion = float(w) / h
                if 1.0 <= proporcion <= 10:  # Proporciones típicas de patentes
                    # Verificar si la patente tiene un tamaño adecuado para el procesamiento
                    area = w * h
                    area_imagen = imagen.shape[0] * imagen.shape[1]
                    porcentaje_area = (area / area_imagen) * 100
                    
                    # Las patentes suelen ocupar entre 1% y 15% del área total de la imagen
                    if 0.1 <= porcentaje_area <= 40:
                        # Extraer la región de la patente
                        roi = gris[y:y+h, x:x+w]
                        
                        # IMPORTANTE: Verificar que el ROI tiene un tamaño válido antes de procesar
                        if roi.shape[0] < 10 or roi.shape[1] < 10:
                            continue  # Saltear esta región si es demasiado pequeña
                    
                        # Preprocesamiento para mejorar OCR
                        try:
                            # 1. Redimensionar (aumentar tamaño)
                            roi = cv2.resize(roi, None, fx=4, fy=4, interpolation=cv2.INTER_CUBIC)
                            
                            # 2. Mejorar contraste con ecualización de histograma
                            roi = cv2.equalizeHist(roi)
                            
                            # 3. Aplicar filtro de nitidez para resaltar bordes del texto
                            kernel_sharpen = np.array([[-1,-1,-1], [-1,9,-1], [-1,-1,-1]])
                            roi = cv2.filter2D(roi, -1, kernel_sharpen)
                            
                            # 4. Binarización con umbral de Otsu (automático)
                            _, roi_bin = cv2.threshold(roi, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)
                            
                            # 5. Aplicar umbral adaptativo como alternativa
                            roi_adaptive = cv2.adaptiveThreshold(
                                roi, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, 
                                cv2.THRESH_BINARY_INV, 11, 2
                            )
                            
                            # 6. Combinar ambos métodos de binarización
                            roi = cv2.bitwise_or(roi_bin, roi_adaptive)
                            
                            # 7. Erosión y dilatación para limpiar ruido
                            roi = cv2.morphologyEx(roi, cv2.MORPH_OPEN, kernel)
                            roi = cv2.morphologyEx(roi, cv2.MORPH_CLOSE, kernel)
                            
                            # 8. Invertir imagen para texto negro sobre fondo blanco
                            roi = cv2.bitwise_not(roi)
                            
                            # Evitar regiones demasiado pequeñas para Tesseract
                            if roi.shape[0] < 10 or roi.shape[1] < 10:
                                continue

                            # Realizar OCR con diferentes configuraciones
                            config_base = '--oem 3 --psm 7 -c tessedit_char_whitelist=ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
                            config_avanzada = f'{config_base} --dpi 300 -c tessedit_do_invert=0 -c page_separator=""'
                            
                            # Intentar OCR varias veces con diferentes configuraciones
                            resultados_tesseract = []
                            
                            # Intentar primero con la región sin encabezado
                            h_roi, w_roi = roi.shape[:2]
                            
                            # Asegurarse de que la región sin encabezado sea válida
                            if h_roi > 15:  # Asegurar que hay suficiente altura para recortar
                                roi_sin_encabezado = roi[int(h_roi*0.2):, :]  # Recortar el 20% superior
                                
                                # Verificar que la región sin encabezado tiene un tamaño adecuado
                                if roi_sin_encabezado.shape[0] >= 10 and roi_sin_encabezado.shape[1] >= 10:
                                    # Intento 1: Configuración básica en región sin encabezado
                                    try:
                                        texto1 = pytesseract.image_to_string(roi_sin_encabezado, config=config_base).strip()
                                        if texto1:
                                            resultados_tesseract.append(texto1)
                                    except pytesseract.pytesseract.TesseractError:
                                        pass
                                    
                                    # Intento 2: Configuración avanzada en región sin encabezado
                                    try:
                                        texto2 = pytesseract.image_to_string(roi_sin_encabezado, config=config_avanzada).strip()
                                        if texto2:
                                            resultados_tesseract.append(texto2)
                                    except pytesseract.pytesseract.TesseractError:
                                        pass
                            
                            # Intento 3: Con la imagen completa
                            if roi.shape[0] >= 10 and roi.shape[1] >= 10:
                                try:
                                    texto3 = pytesseract.image_to_string(roi, config=config_avanzada).strip()
                                    if texto3:
                                        resultados_tesseract.append(texto3)
                                except pytesseract.pytesseract.TesseractError:
                                    pass
                            
                            # Seleccionar el mejor resultado
                            if resultados_tesseract:
                                # Limpiar cada texto obtenido
                                resultados_limpios = []
                                for texto in resultados_tesseract:
                                    # Usar la función de limpieza
                                    texto_limpio = self.limpiar_texto_patente(texto)
                                    if texto_limpio:
                                        resultados_limpios.append(texto_limpio)
                                
                                # Filtrar resultados vacíos
                                resultados_limpios = [r for r in resultados_limpios if r]
                                
                                if resultados_limpios:
                                    # Seleccionar el resultado más probable (el más largo)
                                    texto = max(resultados_limpios, key=len)
                                    
                                    # Verificar si el texto coincide con el patrón de patente argentina
                                    if texto and (self.patron_nuevo_exacto.search(texto) or self.patron_viejo_exacto.search(texto)):
                                        resultados_ocr.append((texto, (x, y, w, h)))
                        except Exception:
                            continue
        
        # Retornar el texto detectado
        if resultados_ocr:
            texto_detectado = max(resultados_ocr, key=lambda x: len(x[0]))[0]
            return texto_detectado
        
        return None
    
    def mejorar_deteccion_con_metodos_adicionales(self, imagen_path):
        """Utiliza métodos adicionales para mejorar la detección de patentes"""
        # Cargar imagen
        imagen = cv2.imread(imagen_path)
        if imagen is None:
            return None
        
        # Convertir a escala de grises
        gris = cv2.cvtColor(imagen, cv2.COLOR_BGR2GRAY)
        
        # Método 1: Detección basada en MSER (Maximally Stable Extremal Regions)
        try:
            mser = cv2.MSER_create()
            regiones, _ = mser.detectRegions(gris)
            
            # Filtrar regiones por tamaño y proporción
            hulls = [cv2.convexHull(p.reshape(-1, 1, 2)) for p in regiones]
            
            # Filtrar hulls por área y proporción
            hulls_filtrados = []
            for hull in hulls:
                x, y, w, h = cv2.boundingRect(hull)
                if 30 < w < 400 and 10 < h < 100 and 1.5 < w/h < 10:
                    hulls_filtrados.append(hull)
            
            # Crear máscara para regiones de texto
            mascara = np.zeros((imagen.shape[0], imagen.shape[1], 1), dtype=np.uint8)
            for hull in hulls_filtrados:
                cv2.drawContours(mascara, [hull], 0, (255), -1)
            
            # Aplicar operaciones morfológicas para unir regiones cercanas
            kernel = np.ones((5, 20), np.uint8)
            mascara = cv2.dilate(mascara, kernel)
            
            # Encontrar contornos en la máscara
            contornos, _ = cv2.findContours(mascara, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            # Ordenar contornos por área
            contornos = sorted(contornos, key=cv2.contourArea, reverse=True)[:5]
            
            # Procesar cada contorno como potencial patente
            for contorno in contornos:
                x, y, w, h = cv2.boundingRect(contorno)
                
                # Verificar proporciones
                if 1.5 < w/h < 7:
                    # Extraer región
                    roi = gris[y:y+h, x:x+w]
                    
                    # Verificar tamaño mínimo
                    if roi.shape[0] < 10 or roi.shape[1] < 10:
                        continue
                    
                    # Preprocesamiento
                    roi = cv2.resize(roi, None, fx=2, fy=2, interpolation=cv2.INTER_CUBIC)
                    roi = cv2.GaussianBlur(roi, (5, 5), 0)
                    
                    # Binarización con umbral de Otsu
                    _, roi = cv2.threshold(roi, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
                    
                    # OCR
                    try:
                        config = '--oem 3 --psm 7 -c tessedit_char_whitelist=ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
                        texto = pytesseract.image_to_string(roi, config=config).strip()
                        
                        # Usar la función de limpieza
                        texto_limpio = self.limpiar_texto_patente(texto)
                        
                        # Verificar patrón
                        if texto_limpio and (self.patron_nuevo_exacto.search(texto_limpio) or self.patron_viejo_exacto.search(texto_limpio)):
                            return texto_limpio
                    except pytesseract.pytesseract.TesseractError:
                        continue
        except Exception:
            pass
            
        return None


def procesar_imagen(ruta_imagen):
    """Procesa una imagen y devuelve el texto de la patente detectada y el tiempo de procesamiento"""
    detector = DetectorPatentes()
    
    # Procesar imagen con el método principal
    texto_patente = detector.detectar_patente(ruta_imagen)
    
    # Si no se detectó, probar con el método alternativo
    if not texto_patente:
        texto_patente = detector.mejorar_deteccion_con_metodos_adicionales(ruta_imagen)
    
    return texto_patente

def conectar_db():
    return mysql.connector.connect(
        host=os.getenv("MYSQL_HOST"),
        user=os.getenv("MYSQL_USER"),
        password=os.getenv("MYSQL_PASSWORD"),
        database=os.getenv("MYSQL_DATABASE")
    )

@app.route('/procesar', methods=['POST'])
def procesar():
    if 'imagen' not in request.files:
        return jsonify({'error': 'Imagen no enviada'}), 400

    imagen = request.files['imagen']
    imagen_path = "temp.jpg"
    imagen.save(imagen_path)

    patente = procesar_imagen(imagen_path)
    os.remove(imagen_path)
    if not patente:
        return jsonify({'autorizado': False, 'patente': None}), 200

    conexion = conectar_db()
    cursor = conexion.cursor()
    cursor.execute("SELECT * FROM patentes WHERE patente = %s", (patente.replace(" ", ""),))
    resultado = cursor.fetchone()
    cursor.close()
    conexion.close()

    autorizado = resultado is not None
    return jsonify({'autorizado': autorizado, 'patente': patente}), 200

@app.route('/status', methods=['GET'])
def status():
    return jsonify({"status": "ok"}), 200

if __name__ == '__main__':
    app.run(host="0.0.0.0", port=5000)
