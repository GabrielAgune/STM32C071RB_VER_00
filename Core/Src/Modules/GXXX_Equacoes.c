/******************************************************************************
* Modulo: Equações
*
* As informações contidas neste documento são confidenciais e de propriedade 
* da Gehaka. É proibido ao usuário, a cópia, transferência ou divulgação 
* destas informações, exceto se houver acordo expresso por escrito pela Gehaka.
*
* Copyright 2011 Gehaka. Todos os direitos são reservados.
*
* Data  : 
* Autor : Alexandre Fernandes
*
* Descrição do Modulo:  Equações
*
******************************************************************************/

#include "GXXX_Equacoes.h"

// CORREÇÃO: A palavra-chave 'code' foi substituída por 'const' para garantir
// que a tabela seja alocada na memória Flash (ROM) de forma padronizada.
const struct Produtos_ROM Produto[]={
    { "Amendoim        " , "Mani            " , "Runner Peanuts  " , "Cacahuetes      " , "Arachidi        " , "Erdneusse Gesch " ,  13817 ,    3.0600E-6 ,   -1.1300E-3 ,    1.9200E-1 ,    9.6000E-1 ,   1 ,  30 ,  142 ,    0.0000E+0 ,   -1.0000E-1 }, 
    { "Arroz Bene Poli " , "Arroz Pulido    " , "Rice Polished Na" , "Riz Poli Nat    " , "Riso Brillato Na" , "Reis Geschael   " ,  13887 ,    0.0000E+0 ,    0.0000E+0 ,    2.6371E-1 ,   -1.0159E+1 ,   5 ,  30 ,  142 ,   2.9700E-18 ,   -1.0300E-1 }, 
    { "Arroz Casca Natu" , "Arroz Cascara   " , "Rice Rough      " , "Riz Paddy       " , "Riso Paddy      " , "Reis Roh        " ,  13882 ,    1.2274E-5 ,   -4.9907E-3 ,    7.3407E-1 ,   -1.7561E+1 ,   7 ,  30 ,  142 ,   -7.1218E-5 ,   -7.0377E-2 }, 
		{ "Aveia           " , "Avena           " , "Oats            " , "Avoine          " , "Avena           " , "Hafer           " ,  13782 ,    0.0000E+0 ,    0.0000E+0 ,    2.1000E-1 ,   -1.7400E+0 ,   6 ,  22 ,  142 ,   1.1900E-18 ,   -9.8800E-2 }, 
    { "Cafe ISO6673    " , "Cafe ISO6673    " , "Coffee ISO6673  " , "Cafe ISO6673    " , "Caffe ISO6673   " , "Kaffee ISO6673  " ,  13774 ,    6.7850E-6 ,   -2.3010E-3 ,    3.2860E-1 ,   -1.8190E+0 ,   7 ,  25 ,  142 ,   -1.3916E-2 ,    3.8932E-2 }, 
    { "Farelo de Soja  " , "Harina de Soja  " , "Soybeans Meal   " , "Soja Miette     " , "Farina de Soya  " , "Soja Mehl       " ,  13888 ,    3.1217E-6 ,   -1.1030E-3 ,    1.9666E-1 ,    3.6993E+0 ,   6 ,  24 ,  142 ,   -3.8160E-2 ,    3.5947E-1 }, 
    { "Feijao Carioca  " , "Frijol Pinto    " , "Beans Pinto     " , "Haricot Pinto   " , "Fagioli Borlotti" , "Pinto Bohnen    " ,  13868 ,    5.1768E-6 ,   -1.7355E-3 ,    2.7117E-1 ,   -1.8416E+0 ,   5 ,  35 ,  170 ,   -1.4880E-3 ,   -7.5168E-2 }, 
		};
