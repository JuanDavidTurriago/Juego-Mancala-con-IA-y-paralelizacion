FROM nginx:1.27-alpine

COPY index.html   /usr/share/nginx/html/index.html
COPY app.js       /usr/share/nginx/html/app.js
COPY ui-extras.js /usr/share/nginx/html/ui-extras.js
COPY style.css    /usr/share/nginx/html/style.css
COPY nginx.conf   /etc/nginx/conf.d/default.conf

EXPOSE 80
