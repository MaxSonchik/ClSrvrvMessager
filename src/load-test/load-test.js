import http from 'k6/http';
import { check } from 'k6';

export default function() {
  const res = http.get('http://messenger-service.default.svc.cluster.local:80/');
  check(res, { 'status was 200': (r) => r.status == 200 });
}