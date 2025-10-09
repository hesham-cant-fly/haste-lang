#!/usr/bin/env -S clj -M
;; A tool that converts Clojure notation in `ast.clj` into JSON
;; because `zprint` is TOO SLOW

(require '[clojure.java.io :as io]
         '[clojure.edn :as edn])

(defn read-clojure-file [file-path]
  (with-open [r (io/reader file-path)]
    (let [pbr (java.io.PushbackReader. r)]
      (loop [forms []]
        (let [form (try
                     (read pbr false ::eof) ;; returns ::eof at EOF
                     (catch Exception e
                       (throw (ex-info "Error reading form" {:cause e}))))]
          (if (= form ::eof)
            forms
            (recur (conj forms form))))))))
;; (defn jsonify [x]
;;   (cond
;;     (map? x)
;;     (str "{" (clojure.string/join ","
;;            (map (fn [[k v]]
;;                   (str "\"" (name k) "\":" (jsonify v)))
;;                 x)) "}")

;;     (list? x)
;;     (str "\"" (name (first x)) "\": {"
;;          (clojure.string/join "," (map jsonify (rest x)))
;;          "}")

;;     (vector? x)
;;     (str "[" (clojure.string/join "," (map jsonify x)) "]")

;;     (string? x) (pr-str x)
;;     (keyword? x) (str "\"" (name x) "\"")
;;     (symbol? x)  (str "\"" (name x) "\"")
;;     (nil? x) "null"
;;     (boolean? x) (if x "true" "false")
;;     :else (str x)))
(defn jsonify [x]
  (cond
    ;; map → standard object
    (map? x)
    (str "{"
         (clojure.string/join ","
           (map (fn [[k v]]
                  (str "\"" (name k) "\":" (jsonify v)))
                x))
         "}")

    ;; list → (foo bar baz ...) → "foo": { ... }
    (list? x)
    (let [k (first x)
          v (rest x)]
      (str "\"" (name k) "\":"
           (if (= 1 (count v))
             ;; one child, just emit it
             (jsonify (first v))
             ;; multiple children → treat as map-like {child1, child2, ...}
             (str "{"
                  (clojure.string/join ","
                    (map (fn [child] (jsonify child)) v))
                  "}"))))

    ;; vector → JSON array
    (vector? x)
    (str "[" (clojure.string/join "," (map jsonify x)) "]")

    ;; scalars
    (string? x) (pr-str x)
    (keyword? x) (str "\"" (name x) "\"")
    (symbol? x)  (str "\"" (name x) "\"")
    (nil? x) "null"
    (boolean? x) (if x "true" "false")
    :else (str x)))


(def path "./build/ast.clj")
(def lispy-shi-huh (read-clojure-file path))
(doseq [x lispy-shi-huh]
  (println (str "{" (jsonify x) "}")))
