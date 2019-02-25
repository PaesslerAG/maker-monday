#!/usr/bin/env python
# coding: utf-8

# In[41]:


from PIL import Image 
import numpy
import os

X_train_true = []
X_train_false = []
y_train = [1 for i in range(0,18)] + [0 for x in range(0,18)]



# In[42]:


for file in os.listdir('images/train/'):
    try:
        image_array = numpy.array(Image.open('images/train/'+file).convert('L'))
        if 'true' in file:
            X_train_true.append(image_array.flatten())
        else:
            X_train_false.append(image_array.flatten())
    except:
        continue
        
X_train = X_train_true + X_train_false


# In[43]:


X_test_true = []
X_test_false = []
y_test = [1 for i in range(0,5)] + [0 for x in range(0,5)]


# In[44]:


for file in os.listdir('images/test/'):
    try:
        image_array = numpy.array(Image.open('images/test/'+file).convert('L'))
        if 'true' in file:
            X_test_true.append(image_array.flatten())
        else:
            X_test_false.append(image_array.flatten())
    except:
        continue

X_test = X_test_true + X_test_false


# In[45]:


from sklearn.tree import DecisionTreeClassifier

dtc = DecisionTreeClassifier(random_state=25)

dtc.fit(X_train, y_train)
y_pred = dtc.predict(X_test)
score=accuracy_score(y_test, y_pred)
print(score)







# In[46]:


from sklearn.linear_model import LogisticRegression

log_reg = LogisticRegression(multi_class="multinomial", solver="lbfgs", random_state=10)
log_reg.fit(X_train, y_train)

y_pred = log_reg.predict(X_test)
score=accuracy_score(y_test, y_pred)
print(score)


# In[47]:


from sklearn.naive_bayes import GaussianNB

gnb = GaussianNB()

gnb.fit(X_train, y_train)
y_pred = gnb.predict(X_test)
score=accuracy_score(y_test, y_pred)
print(score)


# In[50]:



from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score



#rnd_clf = RandomForestClassifier(random_state=0)
rnd_clf = RandomForestClassifier(random_state=25)
rnd_clf.fit(X_train, y_train)

y_pred = rnd_clf.predict(X_test)
score=accuracy_score(y_test, y_pred)
print(score)


# In[39]:


from sklearn.externals import joblib
joblib.dump(rnd_clf, 'rnd_clf.pkl' ) 


# In[52]:


# Loading our Model for Faster use. :D
rnd_clf2 = joblib.load('rnd_clf.pkl')
y_pred = rnd_clf2.predict(X_test)
score = accuracy_score(y_test, y_pred)


# In[ ]:




