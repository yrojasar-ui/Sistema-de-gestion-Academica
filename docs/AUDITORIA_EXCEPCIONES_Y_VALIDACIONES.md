# Auditoría de `try/catch`, excepciones y validaciones

Fecha: 22 de noviembre de 2026  
Autor: Yoel Alexander Rojas Arguedas — Biblioteca Académica (Universidad CENFOTEC)

Esta auditoría cubre el núcleo C++17, la API HTTP (`HttpController`), la persistencia local y MongoDB (`LocalLibraryRepository`, `MongoLibraryRepository`), el worker AMQP (`biblioteca_worker`, `AmqpBroker`) y el frontend React/TypeScript (`session-client`, `mutation-attempt`, `ui`, `Panels`, `CatalogView`, `App`).

---

## 1. Inventario de validaciones existentes, fallos provocados y cambios realizados

| Área y archivo | Qué validación / manejo existía | Cómo se probó (fallo controlado) | Cambio aplicado y prueba de regresión |
|---|---|---|---|
| **Pre-routing HTTP** (`backend/src/HttpController.cpp:182`) | `try { active = b.usuario(...).activo; } catch (const ErrorDominio&) {}` (captura vacía que ocultaba cualquier `ErrorDominio`) y bloque exterior solo capturaba `ErrorDominio`. | Revisión de código + pruebas de autorización/aislamiento en `test_regresion_validaciones.py` y `test_acceso_api.py`. | Se eliminó la captura vacía: solo `e.status == 404` se trata como usuario ausente; otros errores se propagan. Se añadieron `catch (const PersistenceError&)`, `catch (const std::exception&)` y `catch (...)` en pre-routing con registro sanitizado en `std::cerr` y respuesta JSON uniforme sin detalles internos. |
| **Manejador centralizado HTTP** (`backend/src/HttpController.cpp:650`) | `set_exception_handler` traducía `PersistenceError` (503), `ErrorDominio`, `DuplicateBookId` (409), `invalid_argument`/`Json::exception` (400) y `std::exception` (503), pero **no registraba en logs** los fallos de persistencia ni errores internos inesperados, y no tenía `catch (...)`. | Fallos provocados de escritura en disco solo lectura (`chmod 0500`) y archivo JSON corrupto en `test_regresion_validaciones.py`. | Se conserva el manejo centralizado y se agrega registro técnico sanitizado (`sanitizeErrorText`) en `std::cerr` para `PersistenceError`, `std::exception` y `...`, manteniendo mensajes limpios al cliente y 0 credenciales en logs. |
| **Persistencia local** (`backend/src/LocalLibraryRepository.cpp:58`) | `commit()` llamaba `read(path_) != current_` fuera de `try/catch`; si el archivo en disco estaba corrupto o ilegible, `Json::parse_error` escapaba hacia `HttpController` devolviendo **HTTP 400** (`JSON inválido`) en vez de **HTTP 503** (`PersistenceError`). | Se corrompió externamente el archivo de trabajo en disco antes de enviar `POST /api/mis-pagos/P1/simulacion` (`test_regresion_validaciones.py` paso 4b). | Se envolvió `read(path_)` en `try/catch (const std::exception&)` lanzando `PersistenceError(WriteOutcome::rechazado, ...)`. Ahora responde **HTTP 503** con `resultado=rechazado` y el estado en memoria no cambia. |
| **Idempotencia de pagos y validación de IDs** (`backend/src/LibraryService.cpp`) | `pagar()` entraba siempre a `cambiar()` (reescribiendo el repositorio incluso si el pago ya existía). Varios métodos (`crearEjemplar`, `prestar`, `reservar`, `pagar`, `cancelar`, `anular`, `borrarLibro`, `relacionados`) validaban existencia antes del formato de ID (devolviendo 404 en vez de 400 para IDs inválidos). | Doble clic/reintento sobre `POST /api/mis-pagos/P1/simulacion`, pago sobre cargo anulado (`409`), anulación de cargo ya pagado conservando comprobante, IDs con espacios/símbolos (`400`) en `test_regresion_validaciones.py`. | `pagar()` valida `validarId(id)`, rechaza `anulado` (`409`) y devuelve el comprobante existente sin reescribir ni duplicar aviso cuando ya está `pagado`. Todos los métodos públicos validan `validarId(...)` al entrar. |
| **Arranque CLI y worker** (`backend/apps/catalogo_api.cpp`, `backend/apps/biblioteca_worker.cpp`) | `AMQP_PORT` de entorno usaba `std::stoi` sin validar rango `1..65535`; `catalogo_api.cpp:203` imprimía `e.what()` sin sanitizar; `biblioteca_worker.cpp:278` tenía captura vacía `catch (...) {}` al leer `duplicado`. | Ejecución de `catalogo_api` y `biblioteca_worker` con `AMQP_PORT=abc`, `0` y `99999` (`test_regresion_validaciones.py` paso 1). | Se validó `AMQP_PORT` con `std::from_chars` (`1..65535`), se sanitizó `e.what()` en `catalogo_api.cpp` y se reemplazó `catch (...) {}` por `catch (const Json::exception&)` con log sanitizado en `biblioteca_worker.cpp`. |
| **Error AMQP `channel.close`** (`backend/src/AmqpBroker.cpp`) | Auditado y ya corregido en PR #12 (`2d4952b`): `replyError()` y `methodCloseError()` se construyen antes de `close()` y se evita `std::string(nullptr, 0)`. | Escenario `9b` de `test_rabbitmq_worker.py` bajo ASan/UBSan (`delete_queue` + reconexión). | Conservado y re-verificado en Debug y ASan/UBSan (0 diagnósticos). |
| **Frontend: errores, estado y reconciliación** (`frontend/src/ui.tsx`, `Panels.tsx`, `CatalogView.tsx`, `App.tsx`, `reconciliation-summary.ts`) | `useData.reload()` limpiaba `setData(null)` antes de consultar (borrando la vista actual si fallaba una recarga); `StructuresPanel` y `Tree` mostraban errores con clase verde `success`; casts `(e as Error).message` sin resguardo. | Pruebas unitarias en `domain-helpers.test.ts` y `mutation-attempt.test.ts` (pérdida de respuesta tras pago, error de validación, estado persistido tras recarga). | `errorMessage(unknown)` centralizado; `useData` conserva `data` ante fallos de recarga en la misma ruta; `StructuresPanel` y `Tree` distinguen `problem`/`role="alert"` de `success`; `personalSummary` informa confirmación y comprobante durante reconciliación por GET sin reenviar POST. |

---

## 2. Garantías verificadas frente a fallos controlados

1. **Transiciones inválidas y aislamiento**:
   - Pago sobre cargo anulado → `409 Cargo anulado` (0 comprobantes, 0 avisos).
   - Préstamo sobre ejemplar prestado o apartado para otro lector → `409 Ejemplar no disponible` / `Copia apartada para otro lector`.
   - Cancelación repetida de una reserva → `409 Reserva ya finalizada`.
   - Lector Bruno sobre pagos, préstamos, reservas o avisos de Ana → `404 Registro ausente` (y `403` en rutas administrativas).
2. **Fallo de guardado**:
   - En disco solo lectura (`0500`) o JSON corrupto externamente → `503` con `resultado=rechazado`; el estado en memoria **no se adelanta** al persistido y la UI no muestra confirmación de éxito.
3. **Pérdida de respuesta tras enviar un pago**:
   - `FormDialog` + `createMutationAttempt` bloquea el botón de reenvío, ejecuta únicamente `GET /api/mi-biblioteca` para reconciliar y muestra el comprobante real antes de cerrar.
4. **Fallo de RabbitMQ después de guardar el pago**:
   - El pago y el comprobante `PG-n` quedan persistidos y sobreviven al reinicio; el aviso `pago_simulado` queda en `pendiente` (sin simular éxito) hasta que el broker vuelva a estar disponible.
5. **Logs y secretos**:
   - Los detalles técnicos se escriben únicamente en `stderr` sanitizados; las respuestas HTTP muestran mensajes claros al usuario y las pruebas verifican ausencia total de contraseñas y tokens de servicio en logs.
