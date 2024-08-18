const WIDTH = 1200;
const HEIGHT = 900;

const imagePlane = [
  [ 1, 0.75, 0 ],
  [ -1, 0.75, 0 ],
  [ 1, -0.75, 0 ],
  [ -1, -0.75, 0 ]
];
const camera = [ 0, 0, -1 ];

const shinyMetal = {
  ka: [0.1, 0.1, 0.1], // Ambient color
  kd: 0.3,             // Diffuse reflection coefficient
  ks: 0.9,             // Specular reflection coefficient
  a: 200,              // Shininess coefficient
  kr: scaleVector([0.1, 0.1, 0.1], 10) // Reflection coefficient
};

const mattePlastic = {
  ka: [0.2, 0.2, 0.2], // Ambient color
  kd: 0.8,             // Diffuse reflection coefficient
  ks: 0.2,             // Specular reflection coefficient
  a: 10,               // Shininess coefficient
  kr: scaleVector([0.2, 0.2, 0.2], 5) // Reflection coefficient
};

const glossyPaint = {
  ka: [0.3, 0.3, 0.3], // Ambient color
  kd: 0.6,             // Diffuse reflection coefficient
  ks: 0.6,             // Specular reflection coefficient
  a: 50,               // Shininess coefficient
  kr: scaleVector([0.3, 0.3, 0.3], 5) // Reflection coefficient
};

const rubber = {
  ka: [0.2, 0.2, 0.2], // Ambient color
  kd: 0.9,             // Diffuse reflection coefficient
  ks: 0.1,             // Specular reflection coefficient
  a: 5,                // Shininess coefficient
  kr: scaleVector([0.2, 0.2, 0.2], 5) // Reflection coefficient
};

const glass = {
  ka: [0.1, 0.1, 0.1], // Ambient color
  kd: 0.1,             // Diffuse reflection coefficient
  ks: 0.9,             // Specular reflection coefficient
  a: 300,              // Shininess coefficient
  kr: scaleVector([0.9, 0.9, 0.9], 10) // Reflection coefficient
};

const wood = {
  ka: [0.3, 0.2, 0.1], // Ambient color
  kd: 0.7,             // Diffuse reflection coefficient
  ks: 0.1,             // Specular reflection coefficient
  a: 20,               // Shininess coefficient
  kr: scaleVector([0.3, 0.2, 0.1], 2) // Reflection coefficient
};

const gold = {
  ka: [0.3, 0.2, 0],   // Ambient color
  kd: 0.5,             // Diffuse reflection coefficient
  ks: 0.9,             // Specular reflection coefficient
  a: 100,              // Shininess coefficient
  kr: scaleVector([0.9, 0.7, 0.2], 10) // Reflection coefficient
};

const materials = [
  shinyMetal,
  mattePlastic,
  glossyPaint,
  rubber,
  glass,
  wood,
  gold
];

function getRandom(min, max) {
  return Math.random() * (max - min) + min;
}

const spheres = [];
for (let i = 0; i < 20; i++) {
  const material = materials[Math.floor(Math.random() * materials.length)];
  const color = [Math.random(), Math.random(), Math.random()];
  const center = [getRandom(-3, 3), getRandom(-3, 3), getRandom(1, 5)];
  const radius = getRandom(0.2, 0.7);
  spheres.push({ center, radius, color, material });
}

const lights = [
  { center: [ 1, 1, 1], di: 0.5, si: 0.5 },
  { center: [ -1, 1, 1], di: 0.7, si: 0.3 },
  { center: [ 0, -1, 1], di: 0.3, si: 0.7 },
  { center: [ 2, 2, 2], di: 0.4, si: 0.6 },
  { center: [ -2, 2, 2], di: 0.6, si: 0.4 }
];
ai = 0.1;
const maxDepth = 3;
const samplesPerPixel = 4;


const image = new Image(WIDTH, HEIGHT);
document.image = image;

function addVectors(v1, v2) {
  return [v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2]];
}

function scaleVector(v, scalar) {
  return [v[0] * scalar, v[1] * scalar, v[2] * scalar];
}

function multiplyVectors(v1, v2) {
  if (v1.length !== v2.length) {
    throw new Error("Vectors must be of the same length");
  }

  return v1.map((value, index) => value * v2[index]);
}

function subtractVectors(v1, v2) {
  return [v1[0] - v2[0], v1[1] - v2[1], v1[2] - v2[2]];
}

function normalizeVector(v) {
  const length = Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  return [v[0] / length, v[1] / length, v[2] / length];
}

function dotProduct(v1, v2) {
  return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

function raiseVectorToPower(vector, power) {
  return vector.map(value => Math.pow(value, power));
}

function clamp(num, lower, upper) {
  return Math.min(Math.max(num, lower), upper);
}

function clampVector(v, lower, upper) {
  return [clamp(v[0], lower, upper), clamp(v[1], lower, upper), clamp(v[2], lower, upper)];
}

function raySphereIntersection(rayOrigin, rayDirection, sphere) {
  const c = sphere.center;
  const r = sphere.radius;
  const cPrime = subtractVectors(rayOrigin, c);

  const a = dotProduct(rayDirection, rayDirection);
  const b = 2 * dotProduct(cPrime, rayDirection);
  const cVal = dotProduct(cPrime, cPrime) - r * r;

  const discriminant = b * b - 4 * a * cVal;

  if (discriminant < 0) {
    return null; // No intersection
  }

  const sqrtDiscriminant = Math.sqrt(discriminant);
  const t1 = (-b - sqrtDiscriminant) / (2 * a);
  const t2 = (-b + sqrtDiscriminant) / (2 * a);

  if (t1 > 0 && t2 > 0) {
    return Math.min(t1, t2);
  } else if (t1 > 0) {
    return t1;
  } else if (t2 > 0) {
    return t2;
  }

  return null; // Intersection is behind the camera
}

function traceRay(rayOrigin, rayDirection, spheres, lights, depth) {
  if (depth > maxDepth) {
    console.log("Exceeded max depth at", rayOrigin, rayDirection);
    return [0, 0, 0]; // Return black if maximum recursion depth is reached
  }
  

  let closestT = Infinity;
  let closestSphere = null;

  for (const sphere of spheres) {
    const t = raySphereIntersection(rayOrigin, rayDirection, sphere);
    if (t !== null && t < closestT) {
      closestT = t;
      closestSphere = sphere;
    }
  }

  if (closestSphere) {
    const intersectionPoint = addVectors(rayOrigin, scaleVector(rayDirection, closestT));
    const surfaceNormal = normalizeVector(subtractVectors(intersectionPoint, closestSphere.center));

    let diffuse = [0, 0, 0];
    let specular = [0, 0, 0];

    const material = closestSphere.material;
    let ambient = scaleVector(material.ka, ai);

    for (const light of lights) {
      const lightVector = normalizeVector(subtractVectors(light.center, intersectionPoint));
      const shadowRayOrigin = addVectors(intersectionPoint, scaleVector(surfaceNormal, 0.01)); // Offset to avoid self-intersection
      const shadowRayDirection = lightVector;

      let inShadow = false;
      for (const sphere of spheres) {
        if (sphere !== closestSphere) {
          const t = raySphereIntersection(shadowRayOrigin, shadowRayDirection, sphere);
          if (t !== null && t > 0 && t < 1) {
            inShadow = true;
            break;
          }
        }
      }

      if (!inShadow) {
        const dotNL = dotProduct(surfaceNormal, lightVector);

        if (dotNL > 0) {
          const diffuseComponent = scaleVector(closestSphere.color, material.kd * light.di * dotNL);
          diffuse = addVectors(diffuse, diffuseComponent);

          const reflectance = subtractVectors(scaleVector(surfaceNormal, 2 * dotNL), lightVector);
          const viewVector = normalizeVector(subtractVectors(camera, intersectionPoint));
          const dotRV = Math.max(dotProduct(reflectance, viewVector), 0);

          const specularComponent = scaleVector([1, 1, 1], material.ks * light.si * Math.pow(dotRV, material.a));
          specular = addVectors(specular, specularComponent);
        }
      }
    }

    ambient = clampVector(ambient, 0, 1);
    diffuse = clampVector(diffuse, 0, 1);
    specular = clampVector(specular, 0, 1);

    let color = clampVector(addVectors(addVectors(ambient, diffuse), specular), 0, 1);

/*
    if (material.kr) {
      const reflectionDirection = normalizeVector(scaleVector(rayDirection, -1));
      const reflectanceVector = subtractVectors(scaleVector(scaleVector(surfaceNormal, dotProduct(surfaceNormal, reflectionDirection)), 2), reflectionDirection);
      const reflectionColor = traceRay(intersectionPoint, reflectanceVector, spheres, lights, depth + 1);
      color = addVectors(color, scaleVector(reflectionColor, material.kr[0]));

    }
    */
    if (material.kr) {
      const reflectionDirection = normalizeVector(subtractVectors(rayDirection, scaleVector(surfaceNormal, 2 * dotProduct(rayDirection, surfaceNormal))));
      const reflectionColor = traceRay(addVectors(intersectionPoint, scaleVector(surfaceNormal, 0.01)), reflectionDirection, spheres, lights, depth + 1);
      color = addVectors(color, scaleVector(reflectionColor, material.kr[0]));
    }

    

    return clampVector(color, 0, 1);
  } else {
    return [0, 0, 0]; // Return black if no intersection
  }
}

const subpixelOffsets = {
  4: [
    [-0.25, -0.25], [0.25, -0.25],
    [-0.25, 0.25], [0.25, 0.25]
  ],
  6: [
    [-0.25, -0.25], [0.25, -0.25],
    [-0.25, 0.25], [0.25, 0.25],
    [0, -0.25], [0, 0.25]
  ],
  9: [
    [-0.33, -0.33], [0, -0.33], [0.33, -0.33],
    [-0.33, 0], [0, 0], [0.33, 0],
    [-0.33, 0.33], [0, 0.33], [0.33, 0.33]
  ]
};


for (let y = 0; y < HEIGHT; y++) {
  for (let x = 0; x < WIDTH; x++) {
    let colorSum = [0, 0, 0];

    for (const offset of subpixelOffsets[samplesPerPixel]) {
      let alpha = (x + 0.5 + offset[0]) / (WIDTH - 1);
      let beta = (y + 0.5 + offset[1]) / (HEIGHT - 1);

      // Perform bilinear interpolation
      let t = addVectors(scaleVector(imagePlane[0], 1 - alpha), scaleVector(imagePlane[1], alpha));
      let b = addVectors(scaleVector(imagePlane[2], 1 - alpha), scaleVector(imagePlane[3], alpha));
      let p = addVectors(scaleVector(t, 1 - beta), scaleVector(b, beta));

      // Calculate the direction of the ray from the camera to the point p
      let rayDirection = subtractVectors(p, camera);
      rayDirection = normalizeVector(rayDirection);

      const color = traceRay(camera, rayDirection, spheres, lights, 0);
      colorSum = addVectors(colorSum, color);
    }

    const finalColor = scaleVector(colorSum, 1 / samplesPerPixel);
    image.putPixel(x, y, {
      r: finalColor[0] * 255,
      g: finalColor[1] * 255,
      b: finalColor[2] * 255
    });
  }
}

image.renderInto(document.querySelector('body'));