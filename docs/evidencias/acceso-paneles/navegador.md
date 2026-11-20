# Recorrido observado en navegador real

Navegador integrado de Codex, localhost de WSL, 16 de noviembre de 2026.
Frontend Vite real y backend C++ Debug, archivo de negocio aislado en build.
Viewport estrecho de 367 px, sin contenido horizontal cortado en las capturas.
No se ejecutó una matriz de navegadores ni de tamaños de escritorio.

Preparación: bootstrap ADMIN por comando local. Ana y Bruno aprovisionados
por API autenticada, con contraseñas aleatorias y archivo privado fuera de Git.
La creación de cuentas en formularios no se verificó manualmente en navegador;
sus endpoints están cubiertos por las pruebas HTTP.

1. Entrar Admin: aparecen 16 obras y navegación administrativa.
2. Editar L001: título “Atlas del campus — demo de acceso”, temas campus/mapas.
   El selector de ejemplares refleja el título nuevo.
3. Crear ejemplar E-DEMO de L001 desde el formulario.
4. Cerrar sesión, entrar Ana, solicitar L001 y abrir Mi biblioteca.
   SOL-1 aparece apartada, turno 1, E-DEMO; préstamos activos vacío.
5. Volver como Admin, abrir Solicitudes, Registrar entrega SOL-1.
   Usuario Ana y E-DEMO aparecen preseleccionados. Crear P-DEMO,
   vencimiento 2026-12-01.
6. Entrar Ana, Mis pagos simulados, confirmar simulación.
   Se muestra PG-1, propietario Ana, préstamo P-DEMO, fecha,
   ₡100,00 y SIMULADO. No se piden datos bancarios.
7. Cambiar a Bruno: Mis pagos simulados indica “No hay cargos de demostración”;
   no conserva el comprobante de Ana.
8. Volver a Admin, Préstamos, Registrar devolución P-DEMO.
   El diálogo exige confirmar recepción física.
9. Estructuras → Validar estructuras: “Invariantes válidas en las cinco
   estructuras”. El snapshot B+ contiene la clave normalizada del título editado.
10. Detener backend y arrancarlo con los mismos archivos de negocio y cuentas.
    La sesión anterior deja de servir. Iniciar sesión como Ana:
    P-DEMO devuelto con fecha, SOL-1 atendida, cero activos y cero pendientes.

Las cancelaciones FIFO, anulación, dos copias, usuarios inactivos, accesos HTTP
directos ajenos y fallos inyectados se verificaron mediante pruebas de API y
dominio, no se atribuyen a este recorrido manual.
