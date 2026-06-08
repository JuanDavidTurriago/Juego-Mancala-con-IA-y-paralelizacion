import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  vus: 10,          // 10 Usuarios concurrentes (carga sintética)
  duration: '30s',  // Ejecutar por 30 segundos
};

export default function () {
  // Configura la URL (cambiar a tu Ingress si pruebas la nube)
  const url = __ENV.URL || 'http://localhost:8000/move';
  
  const payload = JSON.stringify({
    board: [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0],
    side: 0,
    depth: 8,
    threads: __ENV.THREADS || 1
  });

  const params = {
    headers: {
      'Content-Type': 'application/json',
    },
  };

  const res = http.post(url, payload, params);

  check(res, {
    'status es 200': (r) => r.status === 200,
  });

  // Pausa mínima para no saturar los sockets de red del cliente
  sleep(0.1);
}
